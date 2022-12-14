#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

#include "clang/Frontend/FrontendActions.h"
// Declares llvm::cl::extrahelp.
#include "llvm/Support/CommandLine.h"
#include "clang/Driver/Options.h"
#include "llvm/Option/OptTable.h"

#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Tooling/Transformer/RewriteRule.h"
#include "clang/Tooling/Transformer/RangeSelector.h"
#include "clang/Tooling/Transformer/Stencil.h"
#include "clang/Tooling/Transformer/Transformer.h"


using namespace clang;
using namespace llvm;
using namespace clang::transformer;
using namespace clang::ast_matchers;
using namespace clang::tooling;
using namespace clang::driver;

#define DEBUG false

// keeps track of how many changes have been made so far
static int changes_count = 0;

// for generating names
static std::string var_base = "__fuzzfix";
static int new_var_count = 0;

std::string get_var_only() {
    return var_base + std::to_string(new_var_count);
}

std::string get_var_and_inc() {
    new_var_count++;
    return get_var_only();
}

// for binding ast nodes
static StringRef if_stmt = "if_stmt";
static StringRef if_cond = "if_cond";

static StringRef case_stmt = "case_stmt";

static StringRef while_stmt = "while_stmt";
static StringRef while_cond = "while_cond";
static StringRef while_body_compound = "while_body_compound";
static StringRef while_body_single = "while_body_single";

static StringRef for_stmt = "for_stmt";
static StringRef for_cond = "for_cond";
static StringRef for_body_compound = "for_body_compound";
static StringRef for_body_single = "for_body_single";


/**************** Rules ****************/

// use the less error-prone traverse mode (strip unuseful AST layers)
//   https://releases.llvm.org/14.0.0/tools/clang/docs/LibASTMatchersReference.html
// However, this mode does not work for conditions that are macros.
// To also capture those, the current design use the non-strip mode for if/else-if, 
//      and strip mode for the rest.
// UPDATE: change back to original design. The original design cannot rewrite macros, but also does 
//         not assign ptr to int (when we have if (ptr)), which generates compiler warnings.

static RewriteRule else_if_rule = makeRule(
    traverse(TK_IgnoreUnlessSpelledInSource,
        ifStmt(
            hasCondition(
                allOf(
                    // cond needs to be an expr, AND not just a single var refering to some decl
                    expr().bind(if_cond),
                    unless(declRefExpr())
                )
            ),
            // `else if` has parent of ifStmt, while nested-if usually does not.
            // The exception is where the inner-if is the only if-body, and is not in {}
            // However, this exception is of the form `if (x) if (y) ...`, which can be handled in
            // the same way as our edits here. So, this rule actually captures else-if + this case.
            hasParent(ifStmt())
        ).bind(if_stmt)
    ),
    {
        // declare the init cond variable
        insertBefore(
            statement(std::string(if_stmt)),
            cat("{\nint ", run([](auto x) {return get_var_and_inc();}), " = ", expression(if_cond), ";\n")
        ),
        // replace cond expr with cond variable
        changeTo(
            node(std::string(if_cond)),
            run([](auto x) {return get_var_only();})
        ),
        // add closing } and end of this if-stmt (the enclosing if-stmt, not this else-if branch)
        insertAfter(
            node(std::string(if_stmt)),
            cat("\n}")
        )
    }
);

/* case 1: if(...). Requires special treatment since we can't declare a var before if here. */
static RewriteRule case_if_rule = makeRule(
    traverse(TK_IgnoreUnlessSpelledInSource,
        ifStmt(
            hasCondition(
                allOf(
                    // cond needs to be an expr, 
                    // AND not just a single var refering to some decl (this part is ignored now)
                    expr().bind(if_cond),
                    unless(declRefExpr())
                )
            ),
            // note that if there is already case 1 :{}, parent of `if` would be CompoundStmt instead
            hasParent(caseStmt().bind(case_stmt)),
            unless(hasParent(ifStmt())) // does not have if parent (i.e. not else-if)
        ).bind(if_stmt)
    ),
    {
        // declare the init cond variable
        insertBefore(
            statement(std::string(if_stmt)),
            // only diff to normal `if`: add `;` before declaration
            cat(";\nint ", run([](auto x) {return get_var_and_inc();}), " = ", expression(if_cond), ";\n")
        ),
        // replace cond expr with cond variable
        changeTo(
            node(std::string(if_cond)),
            run([](auto x) {return get_var_only();})
        )
    }
);

static RewriteRule if_rule = makeRule(
    traverse(TK_IgnoreUnlessSpelledInSource,
        ifStmt(
            hasCondition(
                allOf(
                    // cond needs to be an expr, 
                    // AND not just a single var refering to some decl (this part is ignored now)
                    expr().bind(if_cond),
                    unless(declRefExpr())
                )
            ),
            unless(hasParent(ifStmt())) // does not have if parent (i.e. not else-if)
        ).bind(if_stmt)
    ),
    {
        // declare the init cond variable
        insertBefore(
            statement(std::string(if_stmt)),
            cat("int ", run([](auto x) {return get_var_and_inc();}), " = ", expression(if_cond), ";\n")
        ),
        // replace cond expr with cond variable
        changeTo(
            node(std::string(if_cond)),
            run([](auto x) {return get_var_only();})
        )
    }
);

/**
 * When while body is compound statement
 */
static RewriteRule while_rule = makeRule(
    traverse(TK_IgnoreUnlessSpelledInSource,
        whileStmt(
            hasCondition(
                allOf(
                    expr().bind(while_cond),
                    unless(declRefExpr())
                )
            ),
            // matches body (only compound statement)
            hasBody(compoundStmt().bind(while_body_compound))
        ).bind(while_stmt)
    ),
    {
        // replace cond expr with true
        changeTo(
            node(std::string(while_cond)),
            cat("1")
        ),
        insertBefore(
            statements(std::string(while_body_compound)),
            cat(
                // declare and init cond variable
                "\nint ", run([](auto x) {return get_var_and_inc();}), " = ", expression(while_cond), ";\n",
                // insert break conditioned upon value of cond variable
                "if (!", run([](auto x) {return get_var_only();}), ") break;"
            )
        )
    }
);

/**
 * When while body is a single statement.
 * Due to the limitation in types of edit supported with `ifBound` EditGenerator, only this way
 * (i.e. writing two rules) can handle two types of body and also nested loops.
 */
static RewriteRule while_rule_single = makeRule(
    traverse(TK_IgnoreUnlessSpelledInSource,
        whileStmt(
            hasCondition(
                allOf(
                    expr().bind(while_cond),
                    unless(declRefExpr())
                )
            ),
            // matches body (only non-compound statement)
            hasBody(stmt(unless(compoundStmt())).bind(while_body_single))
        ).bind(while_stmt)
    ),
    {
        // replace cond expr with true
        changeTo(
            node(std::string(while_cond)),
            cat("1")
        ),
        insertBefore(
            statement(std::string(while_body_single)),
            cat(
                // declare and init cond variable
                "{\nint ", run([](auto x) {return get_var_and_inc();}), " = ", expression(while_cond), ";\n",
                // insert break conditioned upon value of cond variable
                "if (!", run([](auto x) {return get_var_only();}), ") break;\n"
            )
        ),
        // for single stmt body, still need to insert }
        insertAfter(
            statement(std::string(while_body_single)),
            cat("\n}")
        )
    }
);

// TODO: do-while loop
// static RewriteRule do_rule;

// TODO: rewrite for loop inc/dec as well

static RewriteRule for_rule = makeRule(
    traverse(TK_IgnoreUnlessSpelledInSource,
        forStmt(
            hasCondition(
                allOf(
                    expr().bind(for_cond),
                    unless(declRefExpr())
                )
            ),
            // matches body (only compound statement)
            hasBody(compoundStmt().bind(for_body_compound))
        ).bind(for_stmt)
    ),
    {
        // replace cond expr with empty (which is imlicitly 1)
        changeTo(
            node(std::string(for_cond)),
            cat("1")
        ),
        insertBefore(
            statements(std::string(for_body_compound)),
            cat(
                // declare and init cond variable
                "\nint ", run([](auto x) {return get_var_and_inc();}), " = ", expression(for_cond), ";\n",
                // insert break conditioned upon value of cond variable
                "if (!", run([](auto x) {return get_var_only();}), ") break;"
            )
        )
    }
);

static RewriteRule for_rule_single = makeRule(
    traverse(TK_IgnoreUnlessSpelledInSource,
        forStmt(
            hasCondition(
                allOf(
                    expr().bind(for_cond),
                    unless(declRefExpr())
                )
            ),
            // matches body (only non-compound statement)
            hasBody(stmt(unless(compoundStmt())).bind(for_body_single))
        ).bind(for_stmt)
    ),
    {
        // replace cond expr with empty (which is implicitly 1)
        changeTo(
            node(std::string(for_cond)),
            cat("1")
        ),
        insertBefore(
            statement(std::string(for_body_single)),
            cat(
                // declare and init cond variable
                "{\nint ", run([](auto x) {return get_var_and_inc();}), " = ", expression(for_cond), ";\n",
                // insert break conditioned upon value of cond variable
                "if (!", run([](auto x) {return get_var_only();}), ") break;\n"
            )
        ),
        // for single stmt body, still need to insert }
        insertAfter(
            statement(std::string(for_body_single)),
            cat("\n}")
        )
    }
);

// order is important, since the matching is done from first to last
static auto rules = applyFirst({
    else_if_rule,
    case_if_rule,
    if_rule,
    while_rule,
    while_rule_single,
    for_rule,
    for_rule_single
});

/**************** Rules END ****************/


// Apply a custom category to all command-line options so that they are the
// only ones displayed.
static cl::OptionCategory ReCondCategory("rewritecond options");

// CommonOptionsParser declares HelpMessage with a description of the common
// command-line options related to the compilation database and input files.
// It's nice to have this help message in all tools.
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);

// A help message for this specific tool can be added afterwards.
static cl::extrahelp MoreHelp("\nFor rewriting conditionals into assignments ...\n");

// more options
static const opt::OptTable &Options = getDriverOptTable();
static cl::opt<std::string>
    OutputFileName("o",
               cl::desc(Options.getOptionHelpText(options::OPT_o)),
               cl::value_desc("file"),
               cl::cat(ReCondCategory));


// AtomicChange consumer
static AtomicChanges Changes;
static void consumer(Expected<AtomicChange> C) {
    if (auto E = C.takeError()) {
        // We must consume the error. Typically one of:
        // - return the error to our caller
        // - toString(), when logging
        // - consumeError(), to silently swallow the error
        // - handleErrors(), to distinguish error types
        llvm::errs() << "Problem with consuming AtomicChange: " << toString(std::move(E)) << "\n";
        return;
    }
    if (DEBUG) std::cout << "AC #" << changes_count << " : "<< C->toYAMLString() << std::endl;
    Changes.push_back(*C);
    changes_count++;
}


int main(int argc, const char **argv) {
    auto ExpectedParser = CommonOptionsParser::create(argc, argv, ReCondCategory);
    if (!ExpectedParser) {
        // Fail gracefully for unsupported options.
        llvm::errs() << ExpectedParser.takeError();
        return 1;
    }
    CommonOptionsParser& OptionsParser = ExpectedParser.get();
    ClangTool Tool(OptionsParser.getCompilations(),
                 OptionsParser.getSourcePathList());

    MatchFinder Finder;
    clang::tooling::Transformer T(rules, consumer);
    T.registerMatchers(&Finder);
    auto Factory = newFrontendActionFactory(&Finder);

    Tool.run(Factory.get());

    std::ifstream in_file(argv[1]);
    std::stringstream buffer;
    buffer << in_file.rdbuf();
    in_file.close();

    auto spec = ApplyChangesSpec();
    spec.Format = ApplyChangesSpec::kAll;
    spec.Style = format::getGoogleStyle(format::FormatStyle::LanguageKind::LK_Cpp);
    auto ChangedCode = applyAtomicChanges(argv[1], buffer.str(), Changes, spec);

    if (!ChangedCode) {
      llvm::errs() << "Applying changes failed: "
                   << llvm::toString(ChangedCode.takeError()) << "\n";
      return 1;
    }

    // write out result
    if (!OutputFileName.empty()) { // write to file
        std::ofstream outfile(OutputFileName);
        if (outfile.is_open()) {   // can write - good path
            outfile << ChangedCode.get() << std::endl;
            outfile.close();
            std::cerr << "Successfully applied " << changes_count << " changes!" << std::endl;
            return 0;
        }
    }
    // write to stdout, if fails to write to file
    std::cerr << "File operation failed / file not specified. Writing to stdout ..." << std::endl;
    std::cout << ChangedCode.get() << std::endl;

    std::cerr << "Successfully applied " << changes_count << " changes!" << std::endl;
    // std::cout << toString(ChangedCode.takeError()) << std::endl;
}
