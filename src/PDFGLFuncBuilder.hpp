#include <algorithm>
#include <sstream>
#include <fstream>
#include <vector>

#include "StmtInfoSet.hpp"
#include "Utils.hpp"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Stmt.h"
#include "llvm/Support/raw_ostream.h"
#include "IEGenLib.hpp"
#include "omega/OmegaLib.hpp"

using namespace clang;

namespace pdfg_c {

class PDFGLFuncBuilder {
   public:
    explicit PDFGLFuncBuilder(ASTContext* Context)
        : Context(Context), largestScheduleDimension(0){};
    // entry point for each function; gather information about its statements
    void processFunction(FunctionDecl* funcDecl) {
        functionName = funcDecl->getQualifiedNameAsString();
        CompoundStmt* funcBody = cast<CompoundStmt>(funcDecl->getBody());
        processBody(funcBody);
        for (auto it = stmtInfoSets.begin(); it != stmtInfoSets.end(); ++it) {
            (*it).zeroPadScheduleDimension(largestScheduleDimension);
        }
    }

    // print collected information to stdout
    void printInfo() {
        llvm::outs() << "Function: " << functionName << "\n";
        Utils::printSmallLine();
        llvm::outs() << "\n";
        llvm::outs() << "Statements:\n";
        for (std::vector<Stmt>::size_type i = 0; i != stmts.size(); ++i) {
            llvm::outs() << "S" << i << ": "
                         << Utils::stmtToString(stmts[i], Context) << "\n";
        }
        llvm::outs() << "\nIteration spaces:\n";
        for (std::vector<StmtInfoSet>::size_type i = 0;
             i != stmtInfoSets.size(); ++i) {
            llvm::outs() << "S" << i << ": "
                         << stmtInfoSets[i].getIterSpaceString() << "\n";
        }
        llvm::outs() << "\nExecution schedules:\n";
        for (std::vector<StmtInfoSet>::size_type i = 0;
             i != stmtInfoSets.size(); ++i) {
            llvm::outs() << "S" << i << ": "
                         << stmtInfoSets[i].getExecScheduleString()
                         << "\n";
        }
        llvm::outs() << "\n";
    }

    // Convert all statements within loops into set strings
    // and send them to iegen
    void addIEGenSets() {
        llvm::outs() << "Adding sets to iegen:\n";
        // Clear all sets and relations
        iegen.clear();
        iegenSets.clear();
        // Go through each statement
        for (std::vector<StmtInfoSet>::size_type i = 0;
             i != stmtInfoSets.size(); ++i) {
            std::ostringstream oss;
            oss << "S" << i;
            // Save the statement number as the name of our set
            iegenSets.push_back(oss.str());
            oss << " := " << stmtInfoSets[i].getIterSpaceString();
            // Add the set to iegen
            llvm::outs() << "Adding " << oss.str() << "\n";
            iegen.add(oss.str());
        }
        llvm::outs() << "\n";
    }

    // Turn all stored iegen sets into code using omega
    void genIEGenSets(std::string inFile) {
        llvm::outs() << "Generating code from iegen sets:\n";
        // Generate output file name from input file name
        int idx = inFile.rfind(".");
        std::string outFile;
        if (idx != std::string::npos) {
            outFile = inFile.substr(0, idx) + "_gen" + inFile.substr(idx);
        } else {
            outFile = inFile + "_gen";
        }

        // Open our file
        std::ofstream out;
        out.open(outFile);

        // TODO: Filter out failed codegens
        // Go through each set added to iegen
        for (const std::string &name : iegenSets) {
            out << omega.codegen(iegen.get(name)) << std::endl;
        }
        out.close();
        llvm::outs() << "\n";
    }

   private:
    std::string functionName;
    ASTContext* Context;
    std::vector<Stmt*> stmts;
    std::vector<StmtInfoSet> stmtInfoSets;
    std::vector<std::string> iegenSets;
    StmtInfoSet currentStmtInfoSet;
    iegen::IEGenLib iegen;
    omglib::OmegaLib omega;
    int largestScheduleDimension;

    // process the body of a control structure
    void processBody(Stmt* stmt) {
        CompoundStmt* asCompoundStmt = dyn_cast<CompoundStmt>(stmt);
        if (asCompoundStmt) {
            for (auto it : asCompoundStmt->body()) {
                processSingleStmt(it);
            }
        } else {
            processSingleStmt(stmt);
        }
    }

    // process one statement, recursing if compound structure
    void processSingleStmt(Stmt* stmt) {
        // fail on disallowed statement types
        if (isa<WhileStmt>(stmt) || isa<CompoundStmt>(stmt) ||
            isa<SwitchStmt>(stmt) || isa<DoStmt>(stmt) ||
            isa<LabelStmt>(stmt) || isa<AttributedStmt>(stmt) ||
            isa<GotoStmt>(stmt) || isa<ContinueStmt>(stmt) ||
            isa<BreakStmt>(stmt)) {
            std::ostringstream errorMsg;
            errorMsg << "Unsupported compound stmt type "
                     << stmt->getStmtClassName();
            Utils::printErrorAndExit(errorMsg.str(), stmt, Context);
        }

        ForStmt* asForStmt = dyn_cast<ForStmt>(stmt);
        if (asForStmt) {
            currentStmtInfoSet.advanceSchedule();
            currentStmtInfoSet.enterFor(asForStmt, Context);
            processBody(asForStmt->getBody());
            currentStmtInfoSet.exitCtrlFlow();
            return;
        }
        IfStmt* asIfStmt = dyn_cast<IfStmt>(stmt);
        if (asIfStmt) {
            currentStmtInfoSet.enterIf(asIfStmt, Context);
            processBody(asIfStmt->getThen());
            currentStmtInfoSet.exitCtrlFlow();
            return;
        }

        currentStmtInfoSet.advanceSchedule();
        addStmt(stmt);
    }

    // add info about a stmt to the builder
    void addStmt(Stmt* stmt) {
        stmts.push_back(stmt);
        largestScheduleDimension =
            std::max(largestScheduleDimension,
                     currentStmtInfoSet.getScheduleDimension());
        stmtInfoSets.push_back(StmtInfoSet(&currentStmtInfoSet));
    }
};
}  // namespace pdfg_c

