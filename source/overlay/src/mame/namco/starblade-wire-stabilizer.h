// license:BSD-3-Clause
// Bounded dequantization of DSP screen coordinates, before projection/rasterization.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <unordered_map>
#include <vector>

class starblade_wire_stabilizer
{
public:
    struct vertex { double x, y, z; };
    using mesh = std::vector<vertex>;
    starblade_wire_stabilizer()
    {
        const char *disabled = std::getenv("STARBLADE_WIRE_STABILIZE");
        m_allowed = !disabled || disabled[0] != '0';
        if (const char *path = std::getenv("STARBLADE_VERTEX_TRACE")) {
            m_trace = std::fopen(path, "w");
            if (m_trace) std::fputs("frame,track,vertex,x,y,z,sx,sy,matched\n", m_trace);
        }
        if (const char *first = std::getenv("STARBLADE_VERTEX_TRACE_FIRST")) m_first = std::atoi(first);
        if (const char *last = std::getenv("STARBLADE_VERTEX_TRACE_LAST")) m_last = std::atoi(last);
    }
    ~starblade_wire_stabilizer() { if (m_trace) std::fclose(m_trace); }
    starblade_wire_stabilizer(const starblade_wire_stabilizer &) = delete;
    starblade_wire_stabilizer &operator=(const starblade_wire_stabilizer &) = delete;
    void clear() { m_previous.clear(); m_current.clear(); m_lookup.clear(); }
    void next_frame(bool enabled)
    {
        ++m_frame;
        m_enabled = enabled && m_allowed;
        if (!m_enabled) { clear(); return; }
        m_previous.swap(m_current); m_current.clear(); m_lookup.clear();
        for (size_t i = 0; i < m_previous.size(); ++i) {
            m_previous[i].used = false;
            m_lookup.emplace(m_previous[i].shape, i);
        }
    }
    mesh apply(uint64_t shape, const mesh &raw)
    {
        mesh output = raw;
        if (!m_enabled || raw.empty() || m_current.size() >= 2048) return output;
        // The DSP packet has vertex indices and topology, but no object ID.
        // Match whole meshes, never individual nearest vertices or draw slots.
        size_t best = m_previous.size();
        double best_score = std::numeric_limits<double>::infinity(), second = best_score;
        const auto range = m_lookup.equal_range(shape);
        for (auto it = range.first; it != range.second; ++it) {
            const auto &candidate = m_previous[it->second];
            if (candidate.raw.size() != raw.size()) continue;
            double score = 0; bool valid = true;
            for (size_t i = 0; i < raw.size(); ++i) {
                const auto &a = raw[i], &b = candidate.raw[i];
                const double dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
                const double zlimit = std::max(32.0, std::abs(b.z) * 0.08);
                if (std::abs(dx) > 8 || std::abs(dy) > 8 || std::abs(dz) > zlimit) { valid = false; break; }
                score += dx * dx + dy * dy + 16 * dz * dz / (zlimit * zlimit);
            }
            if (!valid) continue;
            score /= raw.size();
            if (score < best_score) { second = best_score; best_score = score; best = it->second; }
            else second = std::min(second, score);
        }
        // Also consider already claimed candidates when detecting ambiguity.
        // A disappearing/overlapping instance must not steal another's history.
        const bool matched = best < m_previous.size() && !m_previous[best].used
            && second > best_score * 2 + 1;
        uint64_t track = ++m_track;
        if (matched) {
            auto &previous = m_previous[best]; previous.used = true; track = previous.track;
            for (size_t i = 0; i < raw.size(); ++i) {
                output[i].x += correction(previous.filtered[i].x - raw[i].x);
                output[i].y += correction(previous.filtered[i].y - raw[i].y);
                // Z remains the DSP's value; both face and edge use the same zsort.
            }
        }
        if (m_trace && m_frame >= m_first && m_frame <= m_last)
            for (size_t i = 0; i < raw.size(); ++i)
                std::fprintf(m_trace, "%llu,%llu,%zu,%.0f,%.0f,%.0f,%.6f,%.6f,%d\n",
                    (unsigned long long)m_frame, (unsigned long long)track, i,
                    raw[i].x, raw[i].y, raw[i].z, output[i].x, output[i].y, matched ? 1 : 0);
        m_current.push_back({shape, track, raw, output, false});
        return output;
    }
private:
    static double correction(double delta)
    {
        // 60% current sample; at most 0.45 source pixel (~1.31 HD pixels) of lag.
        // Never extrapolate or blur the image. Stationary vertices settle exactly.
        const double value = std::clamp(delta * 0.4, -0.45, 0.45);
        return std::abs(value) < 0.001 ? 0.0 : value;
    }
    struct history { uint64_t shape, track; mesh raw, filtered; bool used; };
    std::vector<history> m_previous, m_current;
    std::unordered_multimap<uint64_t, size_t> m_lookup;
    bool m_enabled = false, m_allowed = true;
    uint64_t m_frame = 0, m_track = 0;
    int m_first = 1800, m_last = 1860;
    std::FILE *m_trace = nullptr;
};
