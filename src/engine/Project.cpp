#include "Project.h"
#include <algorithm>

namespace maq {

namespace {
int clampi(int v, int lo, int hi) { return std::max(lo, std::min(hi, v)); }
bool producesOutput(EventKind k) { return k != EventKind::Off && k != EventKind::Velocity; }
}  // namespace

Project::Project() {
    selectedRow_.fill(0);
    tmp_.reserve(256);
}

int Project::outputChannel(const UnitOutput& o, int ch) {
    ch = clampi(ch, 0, 15);
    switch (o.mode) {
        case ChannelMode::Forced: return clampi(o.forced, 1, 16) - 1;
        case ChannelMode::Offset: return ((ch + o.offset) % 16 + 16) % 16;
        default: return ch;
    }
}

bool Project::canOverlap(int ua, int ra, int ub, int rb) const {
    if (ua == ub) return false;
    const bool chained = chain.kind == ChainKind::Series && chain.member[static_cast<size_t>(ua)] &&
                         chain.member[static_cast<size_t>(ub)];
    if (!chained) return true;
    return chain.scope == ChainScope::PerRow && ra != rb;  // A1 et B2 peuvent jouer ensemble, pas A1 et B1
}

std::vector<ChannelCollision> Project::collisions() const {
    struct Producer { int unit, row; };
    std::vector<Producer> byChannel[16];
    for (int u = 0; u < kUnits; ++u) {
        if (!active[static_cast<size_t>(u)]) continue;
        for (int r = 0; r < kRows; ++r) {
            const RowConfig& row = units[static_cast<size_t>(u)].rows[static_cast<size_t>(r)];
            if (!producesOutput(row.event.kind)) continue;
            const int c = outputChannel(output[static_cast<size_t>(u)], clampi(row.channel, 1, 16) - 1);
            byChannel[c].push_back({u, r});
        }
    }
    std::vector<ChannelCollision> result;
    for (int c = 0; c < 16; ++c) {
        int mask = 0;
        const auto& list = byChannel[c];
        for (size_t i = 0; i < list.size(); ++i)
            for (size_t j = i + 1; j < list.size(); ++j)
                if (canOverlap(list[i].unit, list[i].row, list[j].unit, list[j].row))
                    mask |= (1 << list[i].unit) | (1 << list[j].unit);
        if (mask) result.push_back({c + 1, mask});
    }
    return result;
}

int Project::activeMemberCount() const {
    std::array<int, kUnits> list{};
    return memberList(list);
}

bool Project::chainProblem(int& unit, int& row) const {
    if (chain.kind != ChainKind::Series || activeMemberCount() < 2) return false;
    for (int u = 0; u < kUnits; ++u) {
        if (!active[static_cast<size_t>(u)] || !chain.member[static_cast<size_t>(u)]) continue;
        for (int r = 0; r < kRows; ++r) {
            const RowConfig& rc = units[static_cast<size_t>(u)].rows[static_cast<size_t>(r)];
            if (rc.event.kind == EventKind::Off) continue;
            if (rc.mode != PlayMode::OneShotSingle && rc.mode != PlayMode::OneShotRetrigger) {
                unit = u; row = r;
                return true;
            }
        }
    }
    return false;
}

bool Project::isPlaying() const {
    for (int u = 0; u < kUnits; ++u)
        if (active[static_cast<size_t>(u)] && units[static_cast<size_t>(u)].isPlaying()) return true;
    return false;
}

// ---------------------------------------------------------------- chaînage

int Project::memberList(std::array<int, kUnits>& list) const {
    int n = 0;
    for (int u = 0; u < kUnits; ++u)
        if (active[static_cast<size_t>(u)] && chain.member[static_cast<size_t>(u)]) list[static_cast<size_t>(n++)] = u;
    return n;
}

void Project::configureChain() {
    for (auto& un : units) { un.chainInsideUnit = true; un.wrapOnr = true; }
    if (chain.kind != ChainKind::Series) return;
    std::array<int, kUnits> list{};
    const int n = memberList(list);
    for (int i = 0; i < n; ++i) {
        Unit& un = units[static_cast<size_t>(list[static_cast<size_t>(i)])];
        if (chain.scope == ChainScope::PerRow) un.chainInsideUnit = false;  // chaque rangée est une passe indépendante
        else un.wrapOnr = false;                                             // la dernière rangée Onr termine l'unité
    }
}

void Project::holdForChain() {
    if (chain.kind != ChainKind::Series) return;
    std::array<int, kUnits> list{};
    const int n = memberList(list);
    for (int i = 1; i < n; ++i) {  // seule la première unité part : les suivantes attendent leur tour
        Unit& un = units[static_cast<size_t>(list[static_cast<size_t>(i)])];
        for (int r = 0; r < kRows; ++r) un.holdRow(r);
    }
}

void Project::applyChain() {
    std::array<int, kUnits> list{};
    const int n = memberList(list);
    if (n < 2) return;

    for (int i = 0; i < n; ++i) {
        const int u = list[static_cast<size_t>(i)];
        Unit& un = units[static_cast<size_t>(u)];
        int done[kRows];
        int nd = 0;
        for (int f : un.finishedRows())
            if (nd < kRows) done[nd++] = f;
        if (nd == 0) continue;

        if (chain.scope == ChainScope::PerRow) {
            for (int d = 0; d < nd; ++d) {
                const int r = done[d];
                for (int k = 1; k <= n; ++k) {  // prochaine unité de la chaîne dont cette rangée est utilisée
                    const int j = i + k;
                    if (!chain.loopToFirst && j >= n) break;  // arrêt en fin de chaîne
                    const int nu = list[static_cast<size_t>(j % n)];
                    if (units[static_cast<size_t>(nu)].rows[static_cast<size_t>(r)].event.kind == EventKind::Off) continue;
                    const size_t from = tmp_.size();
                    units[static_cast<size_t>(nu)].triggerRow(r, tmp_);
                    tag(from, nu);
                    applyLinks(from, nu);
                    break;
                }
            }
        } else if (!un.anyRowRunning()) {  // unité entière : toutes ses passes sont finies
            for (int k = 1; k <= n; ++k) {
                const int j = i + k;
                if (!chain.loopToFirst && j >= n) break;
                const int nu = list[static_cast<size_t>(j % n)];
                bool hasRow = false;
                for (const auto& row : units[static_cast<size_t>(nu)].rows)
                    if (row.event.kind != EventKind::Off) hasRow = true;
                if (!hasRow) continue;
                const size_t from = tmp_.size();
                units[static_cast<size_t>(nu)].triggerUnit(tmp_);
                tag(from, nu);
                applyLinks(from, nu);
                break;
            }
        }
    }
}

// ---------------------------------------------------------------- liens

bool Project::wouldCycle(int index, const ControlLink& link) const {
    bool edge[kUnits][kUnits] = {};
    for (int i = 0; i < kMaxLinks; ++i) {
        const ControlLink& l = (i == index) ? link : controlLinks[static_cast<size_t>(i)];
        if (l.enabled) edge[clampi(l.srcUnit, 0, kUnits - 1)][clampi(l.dstUnit, 0, kUnits - 1)] = true;
    }
    for (int a = 0; a < kUnits; ++a) {  // fermeture transitive : un cycle = une unité qui peut se piloter elle-même
        for (int k = 0; k < kUnits; ++k)
            for (int i = 0; i < kUnits; ++i)
                for (int j = 0; j < kUnits; ++j)
                    if (edge[i][k] && edge[k][j]) edge[i][j] = true;
        if (edge[a][a]) return true;
    }
    for (int a = 0; a < kUnits; ++a)
        if (edge[a][a]) return true;
    return false;
}

bool Project::setControlLink(int index, const ControlLink& link) {
    if (index < 0 || index >= kMaxLinks) return false;
    if (link.enabled) {
        if (link.srcUnit < 0 || link.srcUnit >= kUnits || link.dstUnit < 0 || link.dstUnit >= kUnits) return false;
        if (link.srcUnit == link.dstUnit) return false;
        if (wouldCycle(index, link)) return false;
    }
    controlLinks[static_cast<size_t>(index)] = link;
    return true;
}

void Project::applyLinks(size_t from, int u) {
    for (size_t i = from; i < tmp_.size(); ++i) {
        const MidiMsg m = tmp_[i];
        for (const auto& l : transposeLinks) {
            if (!l.enabled || l.srcUnit != u || l.srcRow != m.row || m.type != MidiMsg::Type::NoteOn) continue;
            if (l.dstUnit < 0 || l.dstUnit >= kUnits) continue;
            units[static_cast<size_t>(l.dstUnit)].setRelativeBase(l.dstRow, m.data1);
        }
        for (const auto& l : controlLinks) {
            if (!l.enabled || l.srcUnit != u || l.srcRow != m.row || m.type != MidiMsg::Type::ProgramChange) continue;
            if (l.dstUnit < 0 || l.dstUnit >= kUnits) continue;
            applyProgramChange(units[static_cast<size_t>(l.dstUnit)], selectedRow_[static_cast<size_t>(l.dstUnit)], m.data1);
        }
    }
}

// Les sources sont jouées avant leurs cibles : une note de transposition agit sur le pas de la même horloge
int Project::tickOrder(std::array<int, kUnits>& ord) const {
    int n = 0;
    for (int u = 0; u < kUnits; ++u)
        if (active[static_cast<size_t>(u)]) ord[static_cast<size_t>(n++)] = u;
    auto pos = [&](int u) {
        for (int i = 0; i < n; ++i)
            if (ord[static_cast<size_t>(i)] == u) return i;
        return -1;
    };
    auto fix = [&](int src, int dst, bool& moved) {
        const int ps = pos(src), pd = pos(dst);
        if (ps < 0 || pd < 0 || src == dst || pd > ps) return;
        const int d = ord[static_cast<size_t>(pd)];
        for (int i = pd; i < ps; ++i) ord[static_cast<size_t>(i)] = ord[static_cast<size_t>(i + 1)];
        ord[static_cast<size_t>(ps)] = d;
        moved = true;
    };
    for (int pass = 0; pass < kUnits; ++pass) {
        bool moved = false;
        for (const auto& l : transposeLinks)
            if (l.enabled) fix(l.srcUnit, l.dstUnit, moved);
        for (const auto& l : controlLinks)
            if (l.enabled) fix(l.srcUnit, l.dstUnit, moved);
        if (!moved) break;
    }
    return n;
}

// ---------------------------------------------------------------- transport et sortie

void Project::tag(size_t from, int unit) {
    for (size_t i = from; i < tmp_.size(); ++i) tmp_[i].unit = unit;
}

void Project::flush(std::vector<MidiMsg>& out) {
    for (MidiMsg m : tmp_) {
        m.channel = outputChannel(output[static_cast<size_t>(clampi(m.unit, 0, kUnits - 1))], m.channel);
        out.push_back(m);
    }
    tmp_.clear();
}

void Project::start() {
    configureChain();
    selectedRow_.fill(0);
    for (int u = 0; u < kUnits; ++u)
        if (active[static_cast<size_t>(u)]) units[static_cast<size_t>(u)].start();
    holdForChain();
}

void Project::continuePlaying() {
    for (int u = 0; u < kUnits; ++u)
        if (active[static_cast<size_t>(u)]) units[static_cast<size_t>(u)].continuePlaying();
}

void Project::stop(std::vector<MidiMsg>& out) {
    tmp_.clear();
    for (int u = 0; u < kUnits; ++u) {
        const size_t from = tmp_.size();
        units[static_cast<size_t>(u)].stop(tmp_);
        tag(from, u);
    }
    flush(out);
}

void Project::panic(std::vector<MidiMsg>& out) {
    tmp_.clear();
    for (int u = 0; u < kUnits; ++u) {
        const size_t from = tmp_.size();
        units[static_cast<size_t>(u)].panic(tmp_);
        tag(from, u);
    }
    flush(out);
}

void Project::tick(std::vector<MidiMsg>& out) {
    tmp_.clear();
    std::array<int, kUnits> order{};
    const int n = tickOrder(order);
    for (int i = 0; i < n; ++i) {
        const int u = order[static_cast<size_t>(i)];
        const size_t from = tmp_.size();
        units[static_cast<size_t>(u)].tick(tmp_);
        tag(from, u);
        applyLinks(from, u);
    }
    if (chain.kind == ChainKind::Series) applyChain();
    flush(out);
}

}  // namespace maq
