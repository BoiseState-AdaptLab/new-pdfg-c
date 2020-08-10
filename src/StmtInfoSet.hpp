#include <memory>
#include <tuple>
#include <vector>
#include <sstream>

#include "Utils.hpp"
#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"
#include "clang/AST/OperationKinds.h"
#include "clang/AST/Stmt.h"
#include "llvm/ADT/StringRef.h"

using namespace clang;

namespace pdfg_c {

// Represents a control flow statements with conditions and an
// iterator if it is a for loop
struct CtrlFlowStmt {
    CtrlFlowStmt (ASTContext* Context, IfStmt* stmt,
                  std::vector<std::shared_ptr<CtrlFlowStmt>> parents = {})
                  : Context(Context), _hasIter(false) {
        if (BinaryOperator* b = cast<BinaryOperator>(stmt->getCond())) {
            addConstraint(b);
        }
        applySubstitutions(parents);
    }
    CtrlFlowStmt (ASTContext* Context, ForStmt* stmt,
                  std::vector<std::shared_ptr<CtrlFlowStmt>> parents = {})
                  : Context(Context), _hasIter(true){
        parseFor(stmt);
        applySubstitutions(parents);
    }

    // TODO: The constraint needs to be simplified
    // Substitute iterator variable with expression
    void applySubstitution(std::string iter, std::string sub, std::string &target) {
        llvm::outs() << "Replacing " << iter << " With " << sub << " In " << target << "\n";
        size_t iterLen = iter.size();
        size_t subLen = sub.size();
        std::ostringstream result;
        int idx = target.find(iter);
        while (idx != std::string::npos && target.size() > 0) {
            // Make sure the character before is not alphabetic or '_'
            bool validPrev = idx == 0 || (!isalpha(target[idx - 1])
                             && target[idx - 1] != '_');
            // Make sure the character after is not alphanumberic or '_'
            int idx2 = idx + iter.size();
            bool validPost = idx2 == target.size() ||
                             (!isalnum(target[idx2]) &&
                              target[idx2] != '_');
            // Add with substitution
            if (validPrev && validPost) {
                result << target.substr(0, idx) << sub;
            // Add without substitution
            } else {
                result << target.substr(0, idx2);
            }
            target = target.substr(idx2);
            // Find next instance
            idx = target.find(iter);
        }
        result << target;
        target = result.str();
    }

    // Substitute iter variable to ensure single increments (+=2 -> ++)
    void applySubstitutions(std::vector<std::shared_ptr<CtrlFlowStmt>> parents) {
        std::string constrs = getConstraints();
        for (auto it = parents.begin(); it != parents.end(); ++it) {
            if ((*it)->hasSubstitution()) {
                // Get iteration variable and substitution expression
                std::string iter = (*it)->getIter();
                std::string sub = (*it)->getSubstitution();
                applySubstitution(iter, sub, constrs);
            }
        }
        _constraints.str(constrs);
    }

    // Print this struct
    void print() {
        if (_hasIter) { llvm::outs() << "[" << _iter << "]: "; }
        llvm::outs() << getConstraints() << "\n";
    }

    // Returns constraints as a string
    std::string getConstraints() {return _constraints.str();}

    // Returns increment amount for this statment
    int getIncr() {return _incr;}

    // Returns iterator variable for this statement
    std::string getIter() {return _iter;}

    // Returns substition if any
    std::string getSubstitution() {return _sub;}

    // Returns if this is a for loop or not
    bool isFor() {return _hasIter;}

    // Returns if this statement performs a substitution
    bool hasSubstitution() {return isFor() && _incr != 1;}

  private:
    ASTContext* Context;
    std::ostringstream _constraints;
    // Iterator variable if applicable
    std::string _iter;
    // Increment value for iter variable
    int _incr;
    // If the increment value != 1, we will need a substitution
    std::string _sub;
    // has iterator
    bool _hasIter;
 
    void addConstraint(BinaryOperator* b) {
        addConstraint(Utils::exprToString(b, Context).str());
    }

    void addConstraint(std::string stmt) {
        if (getConstraints() != "") {
            _constraints << " && ";
        }
        _constraints << stmt;
    }
   
    // Parse ForStmt
    void parseFor(ForStmt* stmt) {
        // Set to true if we can parse the increment statement
        bool canParse = false;
        // Stores whether the loop is incrementing or decrementing
        _incr = 1;
        // Get increment expression
        Expr* incExpr = stmt->getInc();
        if (isa<UnaryOperator>(incExpr)){
            UnaryOperator* inc = cast<UnaryOperator>(incExpr);
            if (inc->isIncrementDecrementOp()) {
                // Check if it is a positive operator (++i or i++)
                _incr = (inc->isIncrementOp() ? 1 : -1);
                canParse = true;
            }
        } else if (isa<BinaryOperator>(incExpr)) {
            BinaryOperator* inc = cast<BinaryOperator>(incExpr);
            // Get increment operator
            BinaryOperatorKind oper = inc->getOpcode();
            // Operator is '+=' or '-='
            if (oper == BO_AddAssign || oper == BO_SubAssign) {
                // Saves increment value
                Expr::EvalResult result;
                // Rhs must be an integer != 0
                if (inc->getRHS()->EvaluateAsInt(result, *Context)) {
                    _incr = result.Val.getInt().getExtValue();
                    // Check if it is a negative operator (-=)
                    if (oper == BO_SubAssign) {_incr *= -1;}
                    canParse = true;
                }
            // Operator is '='
            } else if (oper == BO_Assign && isa<BinaryOperator>(inc->getRHS())) {
                // This is the rhs of the incrrement statement
                // (e.g. with i = i + 1, this is i + 1)
                BinaryOperator* binOp = cast<BinaryOperator>(inc->getRHS());
                // Get the binary operation that conrols loop incrrementing
                BinaryOperatorKind oper = binOp->getOpcode();
                // Get variable being incrremented
                std::string iterStr = Utils::exprToString(inc->getLHS(),
                                                          Context).str();
                // Must be addition or subtraction
                if (binOp->isAdditiveOp()) {
                    // Get our lh and rh expression, also in string form
                    Expr* lhs = binOp->getLHS();
                    Expr* rhs = binOp->getRHS();
                    std::string lhStr = Utils::exprToString(lhs, Context).str();
                    std::string rhStr = Utils::exprToString(rhs, Context).str();
                    // Saves incrrement value
                    Expr::EvalResult result;
                    // Left side is iter var, right side is integer (e.g. i + 1)
                    if ((lhStr.compare(iterStr) == 0 &&
                         rhs->EvaluateAsInt(result, *Context)) ||
                    // Right side is iter var, left side is integer (e.g. 1 + i)
                    // Operation can't be '-' (e.g. 1 - i)
                        (rhStr.compare(iterStr) == 0 && oper != BO_Sub &&
                         lhs->EvaluateAsInt(result, *Context))){

                        _incr = result.Val.getInt().getExtValue();
                        // Check if it is a negative operator (-=)
                        if (oper == BO_SubAssign) {_incr *= -1;}
                        canParse = true;
                    }
                }
            }
        }
        if (!canParse) {
            llvm::outs() << "Cannot parse loop incrementing statement ";
            incExpr->dumpPretty(*Context);
            llvm::outs() << "\n";
            return;
        }

        // Parse initializer expression
        Stmt* initStmt = stmt->getInit();
        if (isa<BinaryOperator>(initStmt)) {
            parseInit(cast<BinaryOperator>(initStmt));
        } else if (isa<DeclStmt>(initStmt)) {
            parseInit(cast<DeclStmt>(initStmt));
        } else {
            return;
        }
 
        // Parse conditions
        if (isa<BinaryOperator>(stmt->getCond())) {
            // Get the binary operator
            BinaryOperator* bo = cast<BinaryOperator>(stmt->getCond());
            // If there is a subsitution, apply that before adding
            if (hasSubstitution()) {
                std::string cond = Utils::exprToString(bo, Context).str();
                applySubstitution(_iter, _sub, cond);
                addConstraint(cond);
            } else {
                addConstraint(bo);
            }
       }
    }

    // Create constraint from for loop initializer and incrementer statements
    void createInitConstraint(std::string initVal) {
        // Write constraint
        std::string output;
        llvm::raw_string_ostream os(output);
        os << _iter << " >= ";
        // If incr != 1, perform this substitution: iter = incr*iter2 + initVal for 0 <= iter2
        if (_incr != 1) {
            os << "0";
            std::ostringstream oss;
            // Simplify -1*iter to -iter
            if (_incr == -1) {oss << "(-";}
            else {oss << "(" << _incr << "*";}
            oss << _iter << " + " << initVal << ")";
            _sub = oss.str();
        } else {os << initVal;}
        addConstraint(os.str());
    }

    // Parse the init statement if it is a binary operator
    void parseInit(BinaryOperator* init) {
        // Get iterator
        _iter = Utils::exprToString(init->getLHS(), Context).str();
        // Create constraint
        createInitConstraint(Utils::exprToString(init->getRHS(), Context).str());
    }

    // Parse the init statement if it is a declaration
    void parseInit(DeclStmt* init) {
        if (VarDecl* initVar = cast<VarDecl>(init->getSingleDecl())) {
            // Get iterator
            _iter = initVar->getNameAsString();
            // Create constraint
            createInitConstraint(Utils::exprToString(initVar->getInit(), Context).str());
        }
    }
};

// one entry in an execution schedule, which may be a variable or a number
struct ScheduleVal {
    ScheduleVal(std::string var) : var(var), valueIsVar(true) {}
    ScheduleVal(int num) : num(num), valueIsVar(false) {}

    std::string var;
    int num;
    bool valueIsVar;
};

// contains associated information for a statement (iteration space and
// execution schedules)
struct StmtInfoSet {
    StmtInfoSet() {}
    StmtInfoSet(StmtInfoSet* other) {
        stmts = other->stmts;
        schedule = other->schedule;
    }

    std::string getIterSpaceString(bool ampersand=false) {
        std::string output, output2, output3;
        llvm::raw_string_ostream os(output);
        llvm::raw_string_ostream iters(output2), conds(output3);
        iters << "[";
        // Write all statements, start at innermost scope (end of vector)
        for (auto it = stmts.begin(); it != stmts.end(); ++it) {
            bool isFor = (*it)->isFor();
            if (it != stmts.begin()) {
                if (isFor) {iters << ",";}
                conds << " && ";
            }

            // Write the conditions            
            conds << (*it)->getConstraints();
            // Write the iterator
            if (isFor) {iters << (*it)->getIter();}
        }
        iters << "]";
        os << "{" << iters.str() << ": " << conds.str() << "}";
        return os.str();
    }

    std::string getExecScheduleString() {
        std::string output;
        llvm::raw_string_ostream os(output);
        os << "{[";
        // Write all iterator variables
        for (auto it = stmts.begin(); it != stmts.end(); ++it) {
            if (!(*it)->isFor()) {continue;}

            if (it != stmts.begin()) {
                os << ",";
            }
            os << (*it)->getIter();
        }
        os << "]->[";
        // Write schedule
        for (auto it = schedule.begin(); it != schedule.end(); ++it) {
            if (it != schedule.begin()) {
                os << ",";
            }
            if ((*it)->valueIsVar) {
                os << (*it)->var;
            } else {
                os << (*it)->num;
            }
        }
        os << "]}";
        return os.str();
    }

    // move statement number forward in the schedule
    void advanceSchedule() {
        if (schedule.empty() || schedule.back()->valueIsVar) {
            schedule.push_back(std::make_shared<ScheduleVal>(0));
        } else {
            int num = schedule.back()->num;
            schedule.pop_back();
            schedule.push_back(std::make_shared<ScheduleVal>(num + 1));
        }
    }

    // get the dimension of this execution schedule
    int getScheduleDimension() { return schedule.size(); }

    // zero-pad this execution schedule up to a certain dimension
    void zeroPadScheduleDimension(int dim) {
        for (int i = getScheduleDimension(); i < dim; ++i) {
            schedule.push_back(std::make_shared<ScheduleVal>(0));
        }
    }

    // enter* and exit* methods add iterators and constraints when entering a
    // new scope, remove when leaving the scope

    void enterFor(ForStmt* forStmt, ASTContext* Context) {
        // Add for loop
        auto stmt = std::make_shared<CtrlFlowStmt>(Context, forStmt, stmts);
        stmts.push_back(stmt);
        // Update schedule
        schedule.push_back(std::make_shared<ScheduleVal>(stmt->getIter()));
    }

    void enterIf(IfStmt* ifStmt, ASTContext* Context) {
        stmts.push_back(std::make_shared<CtrlFlowStmt>(Context, ifStmt, stmts));
    }

    void exitCtrlFlow () {
        if (stmts.back()->isFor()) {
            // Remove place in for loop from schedule
            schedule.pop_back();
            // Remove for loop from schedule
            schedule.pop_back();
        }
        stmts.pop_back();
    }

   private:
    // List of control flow statements
    std::vector<std::shared_ptr<CtrlFlowStmt>> stmts;
    // execution schedule, which begins at 0
    std::vector<std::shared_ptr<ScheduleVal>> schedule; 
};
}  // namespace pdfg_c
