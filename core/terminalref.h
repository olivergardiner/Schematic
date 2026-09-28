#ifndef TERMINALREF_H
#define TERMINALREF_H

#include "identifiers.h"

#include <QHashFunctions>

struct TerminalRef
{
    ComponentId component = kInvalidComponentId;
    TerminalId terminal = kInvalidTerminalId;
};

inline bool operator==(const TerminalRef &a, const TerminalRef &b)
{
    return a.component == b.component && a.terminal == b.terminal;
}

inline bool operator!=(const TerminalRef &a, const TerminalRef &b)
{
    return !(a == b);
}

inline size_t qHash(const TerminalRef &ref, size_t seed = 0) noexcept
{
    return qHashMulti(seed, ref.component, ref.terminal);
}

#endif // TERMINALREF_H
