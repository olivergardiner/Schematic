#ifndef WIREENDPOINT_H
#define WIREENDPOINT_H

#include "terminalref.h"

#include <QHashFunctions>

#include <variant>

using WireEndpoint = std::variant<TerminalRef, NodeId>;

inline WireEndpoint makeTerminalEndpoint(ComponentId component, TerminalId terminal)
{
    return TerminalRef{component, terminal};
}

inline WireEndpoint makeNodeEndpoint(NodeId node)
{
    return node;
}

inline bool isTerminal(const WireEndpoint &endpoint)
{
    return std::holds_alternative<TerminalRef>(endpoint);
}

inline bool isNode(const WireEndpoint &endpoint)
{
    return std::holds_alternative<NodeId>(endpoint);
}

inline size_t qHash(const WireEndpoint &endpoint, size_t seed = 0) noexcept
{
    if (isTerminal(endpoint)) {
        const TerminalRef &ref = std::get<TerminalRef>(endpoint);
        return qHashMulti(seed, 0U, ref.component, ref.terminal);
    }
    return qHashMulti(seed, 1U, std::get<NodeId>(endpoint));
}

#endif // WIREENDPOINT_H
