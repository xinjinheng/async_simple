#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include "clang/Tooling/Tooling.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/Support/CommandLine.h"

using namespace clang;
using namespace clang::tooling;
using namespace llvm;

static cl::OptionCategory SyncToAsyncConverterCategory("Sync to Async Converter Options");

static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);
static cl::extrahelp MoreHelp("This tool converts synchronous asio code to async_simple coroutine code.");

// 访问者类，用于遍历AST并修改代码
class SyncToAsyncVisitor : public RecursiveASTVisitor<SyncToAsyncVisitor> {
public:
    explicit SyncToAsyncVisitor(ASTContext *Context, Rewriter *Rewriter) 
        : Context(Context), Rewriter(Rewriter) {}

    // 访问函数调用表达式
    bool VisitCallExpr(CallExpr *CE) {
        // 获取函数名
        if (FunctionDecl *FD = CE->getDirectCallee()) {
            std::string FuncName = FD->getNameAsString();
            
            // 检查是否是我们需要转换的同步函数
            if (FuncName == "read_some" || FuncName == "write" || FuncName == "accept" || FuncName == "connect") {
                // 检查函数是否在asio_util命名空间中
                if (FD->getDeclContext()->isNamespace() && 
                    dyn_cast<NamespaceDecl>(FD->getDeclContext())->getNameAsString() == "asio_util") {
                    
                    // 转换同步调用为异步调用
                    std::string AsyncFuncName = "async_" + FuncName;
                    
                    // 在调用前添加co_await
                    Rewriter->InsertText(CE->getBeginLoc(), "co_await ", true, true);
                    
                    // 重命名函数
                    Rewriter->ReplaceText(FD->getLocation(), FuncName.length(), AsyncFuncName);
                }
            }
        }
        
        return true;
    }
    
    // 访问函数声明
    bool VisitFunctionDecl(FunctionDecl *FD) {
        // 检查函数是否需要转换为协程
        if (FD->hasBody()) {
            Stmt *Body = FD->getBody();
            
            // 检查函数体中是否包含需要转换的同步调用
            SyncCallFinder Finder;
            Finder.TraverseStmt(Body);
            
            if (Finder.HasSyncCall) {
                // 将函数返回类型改为Lazy<void>
                QualType ReturnType = Context->getQualType(Context->getRecordType(Context->getTagDeclType(Context->getIdentifierTable().get("Lazy"))));
                ReturnType = Context->getLValueReferenceType(ReturnType);
                
                Rewriter->ReplaceText(FD->getReturnTypeSourceRange(), "async_simple::coro::Lazy<void>");
                
                // 检查函数是否已经包含co_return
                CoReturnFinder CoReturnFinder;
                CoReturnFinder.TraverseStmt(Body);
                
                if (!CoReturnFinder.HasCoReturn) {
                    // 在函数末尾添加co_return
                    Rewriter->InsertText(Body->getEndLoc().getLocWithOffset(-1), "\n    co_return;", true, true);
                }
            }
        }
        
        return true;
    }

private:
    ASTContext *Context;
    Rewriter *Rewriter;
    
    // 用于查找同步调用的辅助类
    class SyncCallFinder : public RecursiveASTVisitor<SyncCallFinder> {
    public:
        bool HasSyncCall = false;
        
        bool VisitCallExpr(CallExpr *CE) {
            if (FunctionDecl *FD = CE->getDirectCallee()) {
                std::string FuncName = FD->getNameAsString();
                if (FuncName == "read_some" || FuncName == "write" || FuncName == "accept" || FuncName == "connect") {
                    if (FD->getDeclContext()->isNamespace() && 
                        dyn_cast<NamespaceDecl>(FD->getDeclContext())->getNameAsString() == "asio_util") {
                        HasSyncCall = true;
                    }
                }
            }
            return true;
        }
    };
    
    // 用于查找co_return语句的辅助类
    class CoReturnFinder : public RecursiveASTVisitor<CoReturnFinder> {
    public:
        bool HasCoReturn = false;
        
        bool VisitCoReturnStmt(CoReturnStmt *CRS) {
            HasCoReturn = true;
            return true;
        }
    };
};

// AST消费者类
class SyncToAsyncConsumer : public ASTConsumer {
public:
    explicit SyncToAsyncConsumer(ASTContext *Context, Rewriter *Rewriter) 
        : Visitor(Context, Rewriter) {}

    void HandleTranslationUnit(ASTContext &Context) override {
        // 遍历AST
        Visitor.TraverseDecl(Context.getTranslationUnitDecl());
    }

private:
    SyncToAsyncVisitor Visitor;
};

// 前端动作类
class SyncToAsyncAction : public ASTFrontendAction {
public:
    SyncToAsyncAction() = default;
    
    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, StringRef File) override {
        // 初始化重写器
        Rewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
        return std::make_unique<SyncToAsyncConsumer>(&CI.getASTContext(), &Rewriter);
    }
    
    void EndSourceFileAction() override {
        // 输出修改后的代码
        SourceManager &SM = Rewriter.getSourceMgr();
        std::string Filename = SM.getFileEntryForID(SM.getMainFileID())->getName();
        
        // 保存到文件
        std::error_code EC;
        llvm::raw_fd_ostream Out(Filename + ".async.cpp", EC, llvm::sys::fs::OF_None);
        if (EC) {
            llvm::errs() << "Error opening output file: " << EC.message() << "\n";
            return;
        }
        
        Rewriter.getEditBuffer(SM.getMainFileID()).write(Out);
        Out.close();
        
        llvm::outs() << "Converted file saved to: " << Filename + ".async.cpp" << "\n";
    }
    
private:
    Rewriter Rewriter;
};

// 主函数
int main(int argc, const char **argv) {
    // 解析命令行选项
    CommonOptionsParser OptionsParser(argc, argv, SyncToAsyncConverterCategory);
    ClangTool Tool(OptionsParser.getCompilations(), OptionsParser.getSourcePathList());
    
    // 运行转换工具
    return Tool.run(newFrontendActionFactory<SyncToAsyncAction>().get());
}