#ifndef PDFG_UTILS_HPP
#define PDFG_UTILS_HPP

#include <map>

#include "clang/AST/Expr.h"
#include "clang/AST/OperationKinds.h"
#include "clang/AST/Stmt.h"
#include "llvm/ADT/StringRef.h"

using namespace clang;

namespace pdfg_c {

/*!
 * \class Utils
 *
 * \brief A helper class with various methods for string conversion.
 */
class Utils {
   public:
    //! Print an error to standard error and exit with error status
    static void printErrorAndExit(llvm::StringRef message);

    //! Print an error to standard error, including the context of a
    //! statement in the source code, and exit with error status
    static void printErrorAndExit(llvm::StringRef message, Stmt* stmt);

    //! Print a line (horizontal separator) to standard output
    static void printSmallLine();

    //! Get the source code of a statement as a string
    static std::string stmtToString(Stmt* stmt);

    //! Get the source code of an expression as a string
    static std::string exprToString(Expr* expr);

    //! Get a string representation of a binary operator
    static std::string binaryOperatorKindToString(BinaryOperatorKind bo);

   private:
    //! String representations of valid operators for use in constraints
    static std::map<BinaryOperatorKind, llvm::StringRef> operatorStrings;

    Utils() = delete;
};

}  // namespace pdfg_c

#endif