/// \file ASTDumper.h
/// \brief 树形 AST 输出器定义。

#ifndef YUAN_AST_ASTDUMPER_H
#define YUAN_AST_ASTDUMPER_H

#include <iosfwd>
#include <string>
#include <vector>

namespace yuan {

class ASTNode;

/// \brief AST 树形输出器
///
/// 以稳定、可比对的文本格式输出 AST，适用于 golden 测试。
class ASTDumper {
public:
    explicit ASTDumper(std::ostream& os);

    /// \brief 输出单个 AST 根节点
    void dump(const ASTNode* node);

private:
    struct Child {
        std::string Label;
        const ASTNode* Node = nullptr;
    };

    std::ostream& OS;
    std::vector<bool> PrefixStack;

    void dumpNode(const ASTNode* node, bool isLast, const std::string& edgeLabel);
    void printPrefix(bool isLast, const std::string& edgeLabel);
    std::string formatNodeLabel(const ASTNode* node) const;
    void collectChildren(const ASTNode* node, std::vector<Child>& out) const;

    static void addChild(std::vector<Child>& out, const std::string& label, const ASTNode* node);
};

} // namespace yuan

#endif // YUAN_AST_ASTDUMPER_H
