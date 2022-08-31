#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

#include "clang/Frontend/FrontendActions.h"
// Declares llvm::cl::extrahelp.
#include "llvm/Support/CommandLine.h"

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
static StringRef if_head = "if_head";

static StringRef while_stmt = "while_stmt";
static StringRef while_cond = "while_cond";
static StringRef while_body_compound = "while_body_compound";
static StringRef while_body_single = "while_body_single";


/**************** Rules ****************/

// use the less error-prone traverse mode
//   https://releases.llvm.org/14.0.0/tools/clang/docs/LibASTMatchersReference.html

static RewriteRule else_if_rule = makeRule(
    traverse(TK_IgnoreUnlessSpelledInSource,
        ifStmt(
            stmt().bind(if_stmt), // bind first
            hasCondition(
                allOf(
                    // cond needs to be an expr, AND not just a single var refering to some decl
                    expr().bind(if_cond),
                    unless(declRefExpr())
                )
            ),
            // `else if` has parent of ifStmt, while nested-if usually does not
            // the exception is where the inner-if is the only if-body, and is not in {}
            // However, this exception is of the form `if (x) if (y) ...`, which means we also can
            // put `y` outside outer-if
            hasParent(ifStmt()),
            hasAncestor(
                ifStmt( // this should be the starting if for this else-if
                    has(ifStmt()), // has an if child (i.e. has else-if)
                    unless(hasParent(ifStmt())), // does not have if parent
                    // note: until here, we can still match to an `if` far front which also has
                    // else-if. Example would be : if () { if () {} else if () {} } else if { ... }
                    // To avoid this case, we require that this ancestor should reach current else-if
                    // by only following ifs. Since there is no convenient way of doing so,
                    // we hardcode the cases for a few steps of tracing if.
                    // anyOf(
                        has(ifStmt(equalsBoundNode("if_stmt")))
                        // has(ifStmt(has(ifStmt(equalsBoundNode("if_stmt"))))),
                        // has(ifStmt(has(ifStmt(has(ifStmt(equalsBoundNode("if_stmt"))))))),
                        // has(ifStmt(has(ifStmt(has(ifStmt(has(ifStmt(equalsBoundNode("if_stmt")))))))))
                    // )
                ).bind(if_head)
            )
        )
    ),
    {
        // declare the init cond variable
        insertBefore(
            before(statement(std::string(if_head))),
            cat("int ", run([](auto x) {return get_var_and_inc();}), " = ", expression(if_cond), ";\n")
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
                    // cond needs to be an expr, AND not just a single var refering to some decl
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
            cat("true")
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
            cat("true")
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


static auto rules = applyFirst({
    // else_if_rule,
    if_rule,
    while_rule,
    while_rule_single
});


// Apply a custom category to all command-line options so that they are the
// only ones displayed.
static cl::OptionCategory MyToolCategory("my-tool options");

// CommonOptionsParser declares HelpMessage with a description of the common
// command-line options related to the compilation database and input files.
// It's nice to have this help message in all tools.
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);

// A help message for this specific tool can be added afterwards.
static cl::extrahelp MoreHelp("\nMore help text...\n");

static AtomicChanges Changes;
static void consumer(Expected<AtomicChange> C) {
    std::cout << C->toYAMLString();
    Changes.push_back(*C);
}


int main(int argc, const char **argv) {
    auto ExpectedParser = CommonOptionsParser::create(argc, argv, MyToolCategory);
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

    std::ifstream t(argv[1]);
    std::stringstream buffer;
    buffer << t.rdbuf();

    auto spec = ApplyChangesSpec();
    spec.Format = ApplyChangesSpec::kAll;
    spec.Style = format::getGoogleStyle(format::FormatStyle::LanguageKind::LK_Cpp);
    auto ChangedCode = applyAtomicChanges(argv[1], buffer.str(), Changes, spec);

    if (!ChangedCode) {
      llvm::errs() << "Applying changes failed: "
                   << llvm::toString(ChangedCode.takeError()) << "\n";
      return -1;
    }

    std::cout << ChangedCode.get() << std::endl;
}
