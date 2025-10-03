#include "MidiGenerator.h"
#include "AdvancedHarmonyWindow.h"
#include <random>
#include <algorithm>
#include <cmath>

// --- BEGIN: MidiGenerator getters + helpers definitions ---
int MidiGenerator::getBars() const { return bars_; }
int MidiGenerator::getTSNumerator() const { return tsNum_; }
int MidiGenerator::getTSDenominator() const { return tsDen_; }
int MidiGenerator::getKeySemitone() const { return keySemitone_; }
int MidiGenerator::getScaleIndex() const { return scaleIndex_; }

float MidiGenerator::getHumanizeTiming() const { return humanizeTiming_; }
float MidiGenerator::getHumanizeVelocity() const { return humanizeVelocity_; }
float MidiGenerator::getSwingAmount() const { return swingAmount_; }
float MidiGenerator::getFeelAmount() const { return feelAmount_; }
float MidiGenerator::getRestDensity() const { return restDensity; }

// --- Accent -> Velocity mapping (0..1 accent -> MIDI 1..127)
static uint8_t bang_mapAccentToVelocity(float accent01, uint8_t baseVel, uint8_t range)
{
    if (accent01 < 0.0f) accent01 = 0.0f;
    if (accent01 > 1.0f) accent01 = 1.0f;

    // Center around baseVel with +/- range/2 spread and a gentle ease curve
    const float eased = accent01 * accent01 * (3.0f - 2.0f * accent01); // smoothstep
    int v = (int)std::round((int)baseVel - (int)(range / 2) + eased * (int)range);

    if (v < 1) v = 1;
    if (v > 127) v = 127;
    return (uint8_t)v;
}

// --- Chord helpers: get basic chord tones (root, 3rd, 5th) from scale context
// 'scaleSemis' = scale semitone steps (e.g., Major {0,2,4,5,7,9,11})
static std::array<int, 3> bang_triadSemisFromDegree(int degree0based, const std::vector<int>& scaleSemis)
{
    // Wrap degree into scale length
    const int N = (int)scaleSemis.size();
    const auto sc = [&](int d) { return scaleSemis[(d % N + N) % N]; };

    // Root, +2 scale degrees, +4 scale degrees
    const int r = sc(degree0based);
    const int t = sc(degree0based + 2);
    const int f = sc(degree0based + 4);

    // Convert to (0..11) chord-class semis from the degree root
    return { 0, (t - r + 12) % 12, (f - r + 12) % 12 };
}

static bool bang_isChordToneMidi(int midiPitch, int chordRootMidi, const std::array<int, 3>& triadClass)
{
    const int pc = (midiPitch - chordRootMidi) % 12; // can be negative
    const int norm = (pc + 12) % 12;
    return (norm == triadClass[0] || norm == triadClass[1] || norm == triadClass[2]);
}

static int bang_snapToNearestChordTone(int midiPitch, int chordRootMidi, const std::array<int, 3>& triadClass)
{
    int best = midiPitch;
    int bestAbs = 128;
    for (int o = -2; o <= 2; ++o)
    {
        const int root = chordRootMidi + o * 12;
        for (int cls : { triadClass[0], triadClass[1], triadClass[2] })
        {
            const int cand = root + cls;
            const int d = std::abs(cand - midiPitch);
            if (d < bestAbs) { bestAbs = d; best = cand; }
        }
    }
    return best;
}

// --- Timing humanization clamps + end-snap to keep lines clean
static void bang_applyTimingAndEnds(std::vector<Note>& notes,
    int beatsPerBar,
    float swing01,
    float timeHuman01,
    float velHuman01,
    double minLenBeats)
{
    if (beatsPerBar <= 0) return;

    // Convert to small, safe ranges (beats). 120bpm -> 1 beat = 500ms.
// Clamp timing offset range (beats)
    const float maxTimingBeatLoose = 0.03f;  // ~15ms at 120bpm
    const float maxTiming = juce::jmap(timeHuman01, 0.0f, 1.0f,
        0.0f, maxTimingBeatLoose);

    // Swing shifts only the off-beats (8th swing by default)
    const float swingAmt = juce::jmap(swing01, 0.0f, 1.0f,
        0.0f, 0.16f); // up to ~16% of an 8th

    auto isOffbeat8th = [&](double beatPos)->bool
    {
        const double frac = std::fmod(beatPos, 1.0);
        return (std::abs(frac - 0.5) < 1e-4);
    };

    // 1) Micro-timing
    for (auto& n : notes)
    {
        // Humanize start
        const float jitter = juce::jmap(juce::Random::getSystemRandom().nextFloat(),
            -maxTiming, maxTiming);
        n.startBeats += (double)jitter; // cast once when adding to startBeats

        // Swing off-beats (8ths only)
        if (isOffbeat8th(n.startBeats))
        {
            n.startBeats += swingAmt * 0.5; // shift later towards triplet feel
        }
    }

    // 2) End-snap (avoid mushy overlaps): slight legato or clear gap
    std::sort(notes.begin(), notes.end(),
        [](const Note& a, const Note& b) { return a.startBeats < b.startBeats; });

    const double minGap = 0.03;   // ~15ms
    const double maxLeg = 0.02;   // ~10ms

    for (size_t i = 0; i + 1 < notes.size(); ++i)
    {
        auto& a = notes[i];
        auto& b = notes[i + 1];
        const double aEnd = a.startBeats + a.lengthBeats;

        if (aEnd > b.startBeats)
        {
            // Prefer a tiny legato or a clear gap
            const double desiredEnd = (juce::Random::getSystemRandom().nextFloat() < 0.5f)
                ? (b.startBeats - maxLeg)
                : (b.startBeats - minGap);

            a.lengthBeats = std::max(0.125, desiredEnd - a.startBeats);
        }
    }

    // 3) Velocity humanize around the (already set) velocity
    const int velSpread = (int)juce::jmap(velHuman01, 0.0f, 1.0f, 0.0f, 10.0f); // +/- up to 10
    for (auto& n : notes)
    {
        int v = (int)n.velocity + juce::Random::getSystemRandom().nextInt({ -velSpread, velSpread });
        if (v < 1) v = 1;
        if (v > 127) v = 127;
        n.velocity = (uint8_t)v;
    }
}

// === Phrase & cadence helpers =================================================

// Global-ish melody note counter to control rare fast-notes.
// Safe as internal static; persists across calls.
static uint64_t g_bangMelodyNoteCounter = 0;

// Clamp chord rhythms so no note is shorter than an 1/8 note (0.5 beats)
// and ensure all notes in a chord have the same length.
static void bang_limitChordSubdivisionTo8ths(std::vector<Note>& chords, double beatsPerBar)
{
    if (beatsPerBar <= 0.0 || chords.empty()) return;

    // Sort by start time to process chords chronologically
    std::sort(chords.begin(), chords.end(),
        [](const Note& a, const Note& b) { return a.startBeats < b.startBeats; });

    const double minLen = 0.5; // 1/8 note in "beats"
    const double noteGap = 0.01; // small gap between chords to prevent overlap
    const double eps = 1e-6;   // Epsilon for float comparison for start times

    // Group notes by start time (they form a chord)
    std::vector<std::vector<Note*>> chordGroups;
    if (!chords.empty())
    {
        chordGroups.push_back({ &chords[0] });
        for (size_t i = 1; i < chords.size(); ++i)
        {
            if (std::abs(chords[i].startBeats - chordGroups.back().front()->startBeats) < eps)
            {
                chordGroups.back().push_back(&chords[i]);
            }
            else
            {
                chordGroups.push_back({ &chords[i] });
            }
        }
    }

    // Iterate through the chord groups and set a uniform length for each chord
    for (size_t i = 0; i < chordGroups.size(); ++i)
    {
        auto& currentGroup = chordGroups[i];
        double currentChordStartTime = currentGroup.front()->startBeats;

        // Determine the maximum allowed length for the current chord
        double maxLength;
        if (i + 1 < chordGroups.size())
        {
            // There is a next chord, so the max length is the time until it starts
            double nextChordStartTime = chordGroups[i + 1].front()->startBeats;
            maxLength = nextChordStartTime - currentChordStartTime - noteGap;
        }
        else
        {
            // This is the last chord in the sequence. Use the length of the longest note
            // in the chord as a reference, as there's no next chord to collide with.
            double originalLength = 0;
            for (const auto& notePtr : currentGroup) {
                if (notePtr->lengthBeats > originalLength) {
                    originalLength = notePtr->lengthBeats;
                }
            }
            maxLength = originalLength;
        }

        // The final length must be at least the minimum allowed length
        double finalLength = std::max(minLen, maxLength);

        // Apply the same, final length to all notes in the current chord
        for (auto& notePtr : currentGroup)
        {
            notePtr->lengthBeats = finalLength;
        }
    }
}

// Force cadences: every 4th bar (4, 8, 12, ...) use Dominant (V), next bar Tonic (I).
// Also force final bar cadence if song end lands mid-period.
// Uses key PC and treats Dominant as MAJOR triad (classical minor practice).
static void bang_applyCadences(std::vector<Note>& chords, int bars, int beatsPerBar, int keyPc /*0..11*/)
{
    if (bars <= 0 || beatsPerBar <= 0) return;
    if (chords.empty()) return;

    auto inBar = [&](int barIdx, const Note& n) -> bool
    {
        const double barStart = (double)barIdx * (double)beatsPerBar;
        const double barEnd = barStart + (double)beatsPerBar;
        const double nEnd = n.startBeats + n.lengthBeats;
        return (n.startBeats < barEnd && nEnd > barStart);
    };

    // Build bar-buckets of chord notes
    std::vector<std::vector<size_t>> barIdxs((size_t)bars);
    for (size_t i = 0; i < chords.size(); ++i)
    {
        for (int b = 0; b < bars; ++b)
            if (inBar(b, chords[i]))
                barIdxs[(size_t)b].push_back(i);
    }

    auto setBarToTriad = [&](int barIdx, int rootPc) {
        // Overwrite any notes that *start* in this bar to the triad tones (0,4,7)
        const double barStart = (double)barIdx * (double)beatsPerBar;
        const double barEnd = barStart + (double)beatsPerBar;
        for (size_t i : barIdxs[(size_t)barIdx])
        {
            auto& n = chords[i];
            if (n.startBeats + 1e-6 < barStart || n.startBeats >= barEnd) continue;

            // Choose nearest chord tone to current pitch to avoid huge jumps
            const int cls[3] = { 0,4,7 };
            int best = n.pitch, bestD = 999;
            for (int o = -2; o <= 2; ++o)
            {
                for (int k = 0; k < 3; ++k)
                {
                    const int cand = ((rootPc + cls[k]) % 12 + 12) % 12 + o * 12 + (n.pitch / 12) * 12;
                    int d = std::abs(cand - n.pitch);
                    if (d < bestD) { bestD = d; best = cand; }
                }
            }
            n.pitch = best;
        }
    };

    const int Vpc = (keyPc + 7) % 12;
    const int Ipc = keyPc;

    // Every 4th bar → V, following bar → I
    for (int b = 3; b < bars; b += 4)
    {
        setBarToTriad(b, Vpc);
        if (b + 1 < bars)
            setBarToTriad(b + 1, Ipc);
    }

    // Ensure final bar is I (if not already covered)
    setBarToTriad(bars - 1, Ipc);
}

// Capture a 2-beat motif from bar 0 and reuse it with variation.
// Variation rules: bar1 = motif A; bar2 = A transposed to current chord root PC diff;
// bar3 = A with small contour flip; bar4 = cadence-friendly anchor near chord 3rd/root.
static void bang_shapeMelodyWithMotif(std::vector<Note>& melody,
    int bars, int beatsPerBar,
    int keyPc /*0..11*/)
{
    if (melody.empty() || bars <= 0 || beatsPerBar <= 0) return;

    // Collect notes per bar (starting notes only — keeps it simple)
    std::vector<std::vector<size_t>> barNotes((size_t)bars);
    for (size_t i = 0; i < melody.size(); ++i)
    {
        const int barIdx = (int)std::floor(melody[i].startBeats / (double)beatsPerBar);
        if (barIdx >= 0 && barIdx < bars)
            barNotes[(size_t)barIdx].push_back(i);
    }

    // Extract 2-beat motif from bar 0
    const double motifLen = std::min(2.0, (double)beatsPerBar);
    std::vector<int> motifPitches; motifPitches.reserve(16);
    std::vector<double> motifOffsets; motifOffsets.reserve(16);
    const double bar0 = 0.0;

    for (size_t ni : barNotes[0])
    {
        const auto& n = melody[ni];
        const double off = n.startBeats - bar0;
        if (off + 1e-6 < motifLen)
        {
            motifPitches.push_back(n.pitch);
            motifOffsets.push_back(off);
        }
    }
    if (motifPitches.size() < 2) return; // not enough to form a motif; bail politely

    auto transposeToBar = [&](int barIdx, int semis)
    {
        for (size_t ni : barNotes[(size_t)barIdx])
        {
            auto& n = melody[ni];
            const double local = n.startBeats - (double)barIdx * (double)beatsPerBar;

            // If this note aligns with a motif offset (±small tol), transpose
            for (size_t k = 0; k < motifOffsets.size(); ++k)
            {
                if (std::abs(local - motifOffsets[k]) < 1e-3)
                    n.pitch += semis;
            }
        }
    };

    auto smallContourFlipOnBar = [&](int barIdx)
    {
        // Invert direction for small steps (±2) to keep it musical
        auto& idxs = barNotes[(size_t)barIdx];
        for (size_t i = 1; i < idxs.size(); ++i)
        {
            auto& a = melody[idxs[i - 1]];
            auto& b = melody[idxs[i]];
            int interval = b.pitch - a.pitch;
            if (std::abs(interval) <= 2)
                b.pitch = a.pitch - interval; // flip
        }
    };

    // Compute simple bar transposition: align first note PC of each bar to bar0 first note PC
    auto firstPc = [&](int barIdx)->int {
        for (size_t ni : barNotes[(size_t)barIdx])
            return (melody[ni].pitch % 12 + 12) % 12;
        return keyPc;
    };
    const int bar0Pc = firstPc(0);

    // Apply motif plan to bars 1..3 (if exist)
    if (bars >= 2)
    {
        // Bar 1: transpose to match starting PC of bar1
        const int diff = ((firstPc(1) - bar0Pc) % 12 + 12) % 12;
        transposeToBar(1, diff);
    }
    if (bars >= 3)
    {
        // Bar 2: transpose up a diatonic-ish 2 semis (simple pop move)
        transposeToBar(2, 2);
    }
    if (bars >= 4)
    {
        // Bar 3: small contour flip for variation
        smallContourFlipOnBar(3);
    }
}

// Limit melody fast notes: allow 1/16 or faster only on a rare trigger:
// "50% chance every 100 notes" = when (counter % 100 == 0) && coinFlip,
// otherwise expand sub-1/8 notes to 1/8.
// Limit melody fast notes: by default we expand < 1/8 to 1/8 unless a 1/16-heavy mode is active.
// If allowSixteenths=true, we allow 1/16 lengths freely (but still clamp anything below 1/16 up to 1/16).
static void bang_limitMelodyFastNotes(std::vector<Note>& melody, bool allowSixteenths)
{
    if (melody.empty()) return;

    // When 16ths are allowed, enforce a minimum of 1/16 (0.25 beats).
    // Otherwise, enforce a minimum of 1/8 (0.5 beats) like before.
    const double minLen = allowSixteenths ? 0.25 : 0.50;

    for (auto& n : melody)
    {
        if (n.lengthBeats < minLen)
            n.lengthBeats = minLen;
    }
}

// === Looped-phrase maker for melody ===========================================
// Enforces 1/2/4-bar motif loops with optional small variations.
// - bars=4  -> motifBars in {1,2}, repeatChance=0.25f
// - bars=8  -> motifBars in {1,2,4}, repeatChance=0.20f
// Variations (when not an exact-repeat pass):
//   * small transpose (+/- 2 semis) 30%
//   * contour flip for small steps   25%
//   * occasional neighbor-tone nudge 20% (±1 semi on 1–2 notes)
// Notes are re-written per bar by copying the motif's relative offsets & lengths.
// We do NOT create sub-1/8 notes here (your limiter can still run after).

static void bang_applyLoopingPhrases(std::vector<Note>& melody,
    int bars,
    int beatsPerBar,
    int keyPc /*0..11*/)
{
    if (melody.empty() || bars <= 0 || beatsPerBar <= 0)
        return;

    auto& rng = juce::Random::getSystemRandom();

    // ---- Decide motif length and exact-repeat probability
    int motifBars = 1;
    float exactRepeatChance = 0.25f; // default for 4 bars
    if (bars == 4)
    {
        motifBars = (rng.nextBool() ? 1 : 2);
        exactRepeatChance = 0.25f;
    }
    else if (bars == 8)
    {
        int pick = rng.nextInt({ 0,3 }); // 0..2
        motifBars = (pick == 0 ? 1 : (pick == 1 ? 2 : 4));
        exactRepeatChance = 0.20f;
    }
    else
    {
        // Fallback: smallest phrase
        motifBars = 1;
        exactRepeatChance = 0.20f;
    }

    const bool doExactRepeat = rng.nextFloat() < exactRepeatChance;

    // ---- Build per-bar index lists for melody notes that START in each bar
    std::vector<std::vector<size_t>> barIdx((size_t)bars);
    for (size_t i = 0; i < melody.size(); ++i)
    {
        const int b = (int)std::floor(melody[i].startBeats / (double)beatsPerBar);
        if (b >= 0 && b < bars) barIdx[(size_t)b].push_back(i);
    }

    // ---- Capture motif notes from bars [0 .. motifBars-1]
    struct MotifNote { double relStart; double len; int pitch; int vel; };
    std::vector<MotifNote> motif; motif.reserve(64);

    const double motifStart = 0.0;
    const double motifEnd = (double)motifBars * (double)beatsPerBar;

    // Collect notes whose START lies inside the motif window
    for (size_t b = 0; b < (size_t)motifBars; ++b)
    {
        for (size_t idx : barIdx[b])
        {
            const Note& n = melody[idx];
            if (n.startBeats + 1e-6 < motifEnd)
            {
                motif.push_back(MotifNote{
                    n.startBeats - motifStart, n.lengthBeats, n.pitch, n.velocity
                    });
            }
        }
    }
    if (motif.empty())
        return; // nothing to copy

    // Helper: remove all notes that START in [barStart, barEnd)
    auto eraseBarContent = [&](int bar)->void
    {
        const double barStart = (double)bar * (double)beatsPerBar;
        const double barEnd = barStart + (double)beatsPerBar;
        melody.erase(std::remove_if(melody.begin(), melody.end(),
            [&](const Note& n)
            {
                return (n.startBeats + 1e-6 >= barStart && n.startBeats < barEnd);
            }),
            melody.end());
    };

    // Variation helpers
    auto maybeSmallTranspose = [&](int semisBias = 0)->int
    {
        // 30% chance to apply ±2 semis (bias allows gentle upward/downward tendency)
        if (rng.nextFloat() < 0.30f)
            return (rng.nextBool() ? 2 : -2) + semisBias;
        return 0;
    };

    auto applyContourFlipSmallSteps = [&](std::vector<Note>& notesInBar)
    {
        // 25% chance: flip direction for adjacent small intervals (<= 2 semis)
        if (notesInBar.size() < 2 || rng.nextFloat() >= 0.25f) return;
        for (size_t i = 1; i < notesInBar.size(); ++i)
        {
            int iv = notesInBar[i].pitch - notesInBar[i - 1].pitch;
            if (std::abs(iv) <= 2)
                notesInBar[i].pitch = notesInBar[i - 1].pitch - iv;
        }
    };

    auto maybeNeighborNudges = [&](std::vector<Note>& notesInBar)
    {
        // 20% chance: nudge 1–2 random notes by ±1 semi
        if (notesInBar.empty() || rng.nextFloat() >= 0.20f) return;
        const int count = 1 + (int)rng.nextBool(); // 1 or 2
        for (int k = 0; k < count; ++k)
        {
            const size_t i = (size_t)rng.nextInt((int)notesInBar.size());
            notesInBar[i].pitch += (rng.nextBool() ? 1 : -1);
        }
    };

    // Helper to compute transposition to "line up" first note PC vs motif first note PC
    auto computePcAlignSemis = [&](int bar)->int
    {
        // first motif note PC:
        int motifPc = (motif.front().pitch % 12 + 12) % 12;
        // first note PC in target bar (or key if empty)
        int barPc = keyPc;
        if (!barIdx[(size_t)bar].empty())
        {
            const Note& n0 = melody[barIdx[(size_t)bar].front()];
            barPc = (n0.pitch % 12 + 12) % 12;
        }
        int diff = barPc - motifPc;
        // normalize to -6..+5 to avoid huge jumps
        while (diff > 6)  diff -= 12;
        while (diff < -6) diff += 12;
        return diff;
    };

    // ---- Rewrite bars by repeating the motif across the phrase
    std::vector<Note> newNotes;
    newNotes.reserve(melody.size() + bars * motif.size());

    // Keep bars inside motif as-is (we treat bar 0..motifBars-1 as the "source")
    for (size_t b = 0; b < (size_t)motifBars && b < (size_t)bars; ++b)
        for (size_t idx : barIdx[b])
            newNotes.push_back(melody[idx]);

    // For subsequent bars, either exact-copy or copy-with-variation
    for (int bar = motifBars; bar < bars; ++bar)
    {
        const double barStart = (double)bar * (double)beatsPerBar;

        // Decide transposition
        int semis = 0;
        if (!doExactRepeat)
        {
            const int align = computePcAlignSemis(bar);
            semis = align + maybeSmallTranspose();
        }

        // Create the bar notes from motif
        std::vector<Note> built;
        built.reserve(motif.size());
        for (const auto& m : motif)
        {
            Note n;
            n.startBeats = barStart + m.relStart;
            n.lengthBeats = m.len;
            n.pitch = m.pitch + semis;
            n.velocity = m.vel;
            built.push_back(n);
        }

        // Optional variations
        if (!doExactRepeat)
        {
            // Contour flip on small steps
            std::sort(built.begin(), built.end(),
                [](const Note& a, const Note& b) { return a.startBeats < b.startBeats; });
            applyContourFlipSmallSteps(built);
            maybeNeighborNudges(built);
        }

        // Append to newNotes
        newNotes.insert(newNotes.end(), built.begin(), built.end());
    }

    // Replace melody wholesale with the rebuilt list
    melody.swap(newNotes);

    // Rebuild per-bar index (useful if later passes rely on it)
    // (Not strictly necessary—later passes iterate the flat list.)
}

// --- Enforce monophonic melody: never two notes at the same time.
// Strategy: sort by start; if two notes start at the same time, keep the one
// with higher velocity (or the later one if equal). Then remove any overlaps
// by trimming earlier note to end just before the next starts.
static void bang_enforceMonophonic(std::vector<Note>& notes)
{
    if (notes.size() < 2) return;

    // 1) Sort by (startBeats, then velocity desc to prefer keeping stronger hits)
    std::sort(notes.begin(), notes.end(), [](const Note& a, const Note& b)
        {
            if (a.startBeats != b.startBeats) return a.startBeats < b.startBeats;
            if (a.velocity != b.velocity)     return a.velocity > b.velocity;
            return a.lengthBeats > b.lengthBeats; // as a final tiebreaker
        });

    // 2) Collapse identical-start notes (keep first, drop the rest)
    std::vector<Note> collapsed;
    collapsed.reserve(notes.size());

    const double eps = 1e-6;
    for (size_t i = 0; i < notes.size(); )
    {
        size_t j = i + 1;
        // group with the same start
        while (j < notes.size() && std::abs(notes[j].startBeats - notes[i].startBeats) <= eps)
            ++j;
        // keep the first (highest velocity due to sort)
        collapsed.push_back(notes[i]);
        i = j;
    }

    // 3) Trim overlaps: ensure strictly one-at-a-time
    std::sort(collapsed.begin(), collapsed.end(),
        [](const Note& a, const Note& b) { return a.startBeats < b.startBeats; });

    const double minGap = 0.02; // small clear separation
    for (size_t k = 0; k + 1 < collapsed.size(); ++k)
    {
        auto& a = collapsed[k];
        auto& b = collapsed[k + 1];
        const double aEnd = a.startBeats + a.lengthBeats;

        if (aEnd > b.startBeats - minGap)
            a.lengthBeats = std::max(0.125, (b.startBeats - minGap) - a.startBeats);
    }

    notes.swap(collapsed);
}

// --- FINAL GUARD: sanitize notes so there are no blips and no collisions.
// Behavior:
//  * Remove zero/negative-length notes.
//  * Collapse notes that start at the same time (keep the strongest/longest).
//  * Enforce minimum length (minLenBeats). If it would collide with next note:
//      - MONOPHONIC: prefer deleting very short notes; otherwise trim to clear gap.
//      - POLYPHONIC: trim to clear gap; if still < 80% of min, drop the shorter.
//  * Ensure a small gap/legato margin so nothing re-overlaps.
//
// Use minLenBeats = 0.50 for "no smaller than 1/8".
static void bang_finalSanitizeNotes(std::vector<Note>& notes,
    double minLenBeats,
    bool   monophonic)
{
    if (notes.empty()) return;

    const double eps = 1e-6;
    const double minGap = 0.02;                // clear separation floor (beats)
    const double keepIfOver = 0.80 * minLenBeats; // if trimmed below this, drop

    // 0) drop invalid/zero/negative
    notes.erase(std::remove_if(notes.begin(), notes.end(), [&](const Note& n)
        {
            return !(n.lengthBeats > 0.0);
        }), notes.end());
    if (notes.empty()) return;

    // 1) sort by start, then velocity desc, then length desc
    std::sort(notes.begin(), notes.end(), [](const Note& a, const Note& b)
        {
            if (a.startBeats != b.startBeats) return a.startBeats < b.startBeats;
            if (a.velocity != b.velocity)   return a.velocity > b.velocity;
            return a.lengthBeats > b.lengthBeats;
        });

    // 2) collapse equal-starts (keep first)
    std::vector<Note> collapsed;
    collapsed.reserve(notes.size());
    for (size_t i = 0; i < notes.size();)
    {
        size_t j = i + 1;
        while (j < notes.size() && std::abs(notes[j].startBeats - notes[i].startBeats) <= eps)
            ++j;
        collapsed.push_back(notes[i]);
        i = j;
    }

    // 3) enforce floor and clean collisions (no drops; we push/trim to fit)
    std::vector<Note> cleaned;
    cleaned.reserve(collapsed.size());

    for (size_t i = 0; i < collapsed.size(); ++i)
    {
        Note n = collapsed[i];

        // enforce minimum length
        if (n.lengthBeats < minLenBeats)
            n.lengthBeats = minLenBeats;

        // try to avoid collision with next
        const bool hasNext = (i + 1 < collapsed.size());
        if (hasNext)
        {
            Note& next = const_cast<Note&>(collapsed[i + 1]);
            double nEnd = n.startBeats + n.lengthBeats;

            // If we collide or are too close, adjust the NEXT note forward
            if (nEnd > next.startBeats - minGap)
            {
                // For monophonic melody: move next after this note (preserve floor)
                if (monophonic)
                {
                    next.startBeats = nEnd + minGap;
                    if (next.lengthBeats < minLenBeats)
                        next.lengthBeats = minLenBeats;
                }
                else
                {
                    // Polyphonic chords: prefer to trim current to fit, but never below floor.
                    double allowedLen = (next.startBeats - minGap) - n.startBeats;
                    if (allowedLen < minLenBeats)
                    {
                        // not enough room: push next forward minimally
                        next.startBeats = nEnd + minGap;
                        if (next.lengthBeats < minLenBeats)
                            next.lengthBeats = minLenBeats;
                    }
                    else
                    {
                        n.lengthBeats = allowedLen;
                    }
                }
            }
        }

        cleaned.push_back(n);
    }

    notes.swap(cleaned);
}

// --- Scale Lock helpers -----------------------------------------------------

// Returns true if any advanced option that can introduce non-diatonic tones is enabled.

// Snap a MIDI note to the nearest pitch class of the current scale (by semitone distance).

// ===== Scales catalogue (local static, defined in .cpp to avoid ODR/link issues) =====
const std::vector<MidiGenerator::Scale>& MidiGenerator::allScales()
{
    // Construct once, reuse forever
    static const std::vector<Scale> k = {
    {"Major",         {0, 2, 4, 5, 7, 9, 11}},
    {"Natural Minor", {0, 2, 3, 5, 7, 8, 10}},
    {"Harmonic Minor",{0, 2, 3, 5, 7, 8, 11}},
    {"Dorian",        {0, 2, 3, 5, 7, 9, 10}},
    {"Phrygian",      {0, 1, 3, 5, 7, 8, 10}},
    {"Lydian",        {0, 2, 4, 6, 7, 9, 11}},
    {"Mixolydian",    {0, 2, 4, 5, 7, 9, 10}},
    {"Aeolian",       {0, 2, 3, 5, 7, 8, 10}},
    {"Locrian",       {0, 1, 3, 5, 6, 8, 10}},
    {"Locrian ♮6",    {0, 1, 3, 5, 6, 9, 10}},
    {"Ionian #5",     {0, 2, 4, 6, 7, 9, 11}},
    {"Dorian #4",     {0, 2, 3, 6, 7, 9, 10}},
    {"Phrygian Dom",  {0, 1, 3, 5, 7, 9, 10}},
    {"Lydian #2",     {0, 3, 4, 6, 7, 9, 11}},
    {"Super Locrian", {0, 1, 3, 4, 6, 8, 10}},
    {"Dorian b2",     {0, 1, 3, 5, 7, 9, 10}},
    {"Lydian Aug",    {0, 2, 4, 6, 8, 9, 11}},
    {"Lydian Dom",    {0, 2, 4, 6, 7, 9, 10}},
    {"Mixo b6",       {0, 2, 4, 5, 7, 8, 10}},
    {"Locrian #2",    {0, 2, 3, 5, 6, 8, 10}},
    {"8 Tone Spanish", {0, 1, 3, 4, 5, 6, 8, 10}},
    {"Phyrgian ♮3",    {0, 1, 4, 5, 7, 8, 10}},
    {"Blues",         {0, 3, 5, 6, 7, 10}},
    {"Hungarian Min", {0, 3, 5, 8, 11}},
    {"Harmonic Maj(Ethopian)",  {0, 2, 4, 5, 7, 8, 11}},
    {"Dorian b5",     {0, 2, 3, 5, 6, 9, 10}},
    {"Phrygian b4",   {0, 1, 3, 4, 7, 8, 10}},
    {"Lydian b3",     {0, 2, 3, 6, 7, 9, 11}},
    {"Mixolydian b2", {0, 1, 4, 5, 7, 9, 10}},
    {"Lydian Aug2",   {0, 3, 4, 6, 8, 9, 11}},
    {"Locrian bb7",   {0, 1, 3, 5, 6, 8, 9}},
    {"Pentatonic Maj", {0, 2, 5, 7, 8}},
    {"Pentatonic Maj", {0, 2, 5, 7, 8}},
    {"Pentatonic Min", {0, 3, 5, 7, 10}},
    {"Neopolitan Maj", {0, 1, 3, 5, 7, 9, 11}},
    {"Neopolitan Min", {0, 1, 3, 5, 7, 8, 10}},
    {"Spanish Gypsy",  {0, 1, 4, 5, 7, 8, 10}},
    {"Romanian Minor", {0, 2, 3, 6, 7, 9, 10}},
    {"Chromatic",      {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}},
    {"Bebop Major",  {0, 2, 4, 5, 7, 8, 9, 11}},
    {"Bebop Minor", {0, 2, 3, 5, 7, 8, 9, 10}}, 
    };
    return k;
};

static bool findScaleByName(const juce::String& name, juce::String& matchedName)
{
    for (const auto& scale : MidiGenerator::allScales())
    {
        if (juce::String(scale.name).equalsIgnoreCase(name))
        {
            matchedName = scale.name;
            return true;
        }
    }
    return false;
}

const MidiGenerator::Scale& MidiGenerator::getScaleByIndex(int index)
{
    const auto& v = allScales();
    const int clamped = juce::jlimit<int>(0, (int)v.size() - 1, index);
    return v[(size_t)clamped];
}

void MidiGenerator::setScaleIndex(int idx)
{
    const auto& scales = allScales();
    const int total = static_cast<int>(scales.size());
    if (total <= 0)
    {
        // defensive: fall back to Major
        scaleIndex_ = 0;
        scaleName_ = "Major";
        return;
    }

    // Clamp index
    const int clamped = juce::jlimit(0, total - 1, idx);

    scaleIndex_ = clamped;
    scaleName_ = scales[(size_t)clamped].name;
}


// Helper: find a scale name in kAllScales (case-insensitive)

// Assumes you have a global/namespace-scope vector kScales or similar.
// If your scales live elsewhere, adjust this to call your owner.

double MidiGenerator::rand01()
{
    static thread_local std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    return dist(rng);
}
// --- END: MidiGenerator getters + helpers definitions ---


static inline float clamp01(float v) {
    return v < 0.f ? 0.f : (v > 1.f ? 1.f : v);
}

namespace {
    // random helpers compatible with older MSVC modes
    static inline double U(std::mt19937& rng)
    {
        static thread_local std::uniform_real_distribution<double> dist(0.0, 1.0);
        return dist(rng);
    }
    static inline double U() // fallback if some old call sites used U() without args
    {
        static thread_local std::mt19937 local{ std::random_device{}() };
        static thread_local std::uniform_real_distribution<double> dist(0.0, 1.0);
        return dist(local);
    }
} // namespace

// ===== Basics ===============================================================
MidiGenerator::MidiGenerator()
{
    // default deterministic seed
    setSeed(0);

    // Rhythm features ON by default
    allowDotted_ = true;
    allowTriplets_ = true;
}

void MidiGenerator::setKey(int semitonesFromC)
{
    keyRoot = juce::jlimit(0, 127, semitonesFromC);
    // keep the 0..11 pitch class in sync for the getters the generator uses
    keySemitone_ = ((keyRoot % 12) + 12) % 12;
}

void MidiGenerator::setTimeSignature(int beats, int unit)
{
    tsBeats = juce::jmax(1, beats);
    tsNoteValue = juce::jlimit(1, 32, unit);
    // keep the getters' fields in sync
    tsNum_ = tsBeats;
    tsDen_ = tsNoteValue;
}

void MidiGenerator::setBars(int bars)
{
    totalBars = juce::jlimit(1, 128, bars);
    // keep the getters' field in sync
    bars_ = totalBars;
}

void MidiGenerator::setScale(const juce::String& name)
{
    juce::String matched;
    if (findScaleByName(name, matched))
    {
        // Set name
        scaleName_ = matched;

        // Also update index to match the name so UI stays in sync
        int i = 0;
        for (const auto& scale: allScales())
        {
            if (juce::String(scale.name) == matched)
            {
                scaleIndex_ = i;
                break;
            }
            ++i;
        }
    }
    else
    {
        // If not found, keep current and optionally fall back to Major
        // (Uncomment if you want a hard fallback)
        // setScale("Major");
    }
}

void MidiGenerator::setEngineMode(EngineMode m)
{
    engineMode_ = m;
}

// ===== Humanize helpers =====================================================
void MidiGenerator::setHumanizeTiming(float amt) { humanizeTiming_ = juce::jlimit(0.0f, 1.0f, amt); }
void MidiGenerator::setHumanizeVelocity(float amt) { humanizeVelocity_ = juce::jlimit(0.0f, 1.0f, amt); }
void MidiGenerator::setSwingAmount(float amt) { swingAmount_ = juce::jlimit(0.0f, 1.0f, amt); }
void MidiGenerator::setFeelAmount(float amt) { feelAmount_ = juce::jlimit(0.0f, 1.0f, amt); }


// ===== Advanced harmony post-process (safe if advOpts == nullptr) ===========
void MidiGenerator::applyAdvancedHarmonyToChordNotes(std::vector<Note>& chordNotes) const
{
    if (advOpts_ == nullptr) return;
    // Keep this light; your real logic can go here safely using advOpts fields.
    // Example guard usage (no actual field access to avoid compile if header changes):
    (void)chordNotes;
}

// ===== Polyrhythm expansion helper =========================================
std::vector<RhythmStep> MidiGenerator::expandPatternWithPolyrhythm(const RhythmPattern& pat,
    double baseStartBeats) const
{
    std::vector<RhythmStep> out;
    out.reserve(pat.steps.size());

    const double ratio =
        (polyMode == PolyrhythmMode::Ratio3_2) ? 1.5 :
        (polyMode == PolyrhythmMode::Ratio4_3) ? (4.0 / 3.0) :
        (polyMode == PolyrhythmMode::Ratio5_4) ? 1.25 :
        (polyMode == PolyrhythmMode::Ratio7_4) ? 1.75 :
        1.0;

    for (const auto& s : pat.steps)
    {
        RhythmStep rs = s;
        rs.startBeats = baseStartBeats + s.startBeats * ratio;
        rs.lengthBeats = s.lengthBeats * ratio;
        out.push_back(rs);
    }
    return out;
}


void MidiGenerator::setPolyrhythmAmount(float amt01) { polyAmount = juce::jlimit(0.0f, 1.0f, amt01); }

void MidiGenerator::setAdvancedHarmonyOptions(AdvancedHarmonyOptions* opts)
{
    advOpts_ = opts;
}

// --- Scale Lock helpers -----------------------------------------------------

// Returns true if any advanced option that can introduce non-diatonic tones is enabled.
static bool advancedAllowsChromatic(const AdvancedHarmonyOptions* adv)
{
    if (adv == nullptr) return false;

    if (adv->enableSecondaryDominants) return true;
    if (adv->enableChromaticMediants)  return true;
    if (adv->enableNeapolitan)         return true;
    if (adv->enableTritoneSub)         return true;
    if (adv->enableBorrowed)           return true;  // borrowed scale degrees can be non-diatonic
    if (adv->enableAltChords)          return true;  // b5/#5/b9/#9, etc.

    return false; // sus/slash/extensions alone don’t force chromatic PCs
}

// Snap a MIDI note to the nearest pitch class of the current scale (by semitone distance).
static int snapToScalePC(int midi, int rootPC, const std::vector<int>& scaleIntervals)
{
    const int in = juce::jlimit(0, 127, midi);
    const int pc = ((in % 12) + 12) % 12;

    int bestPc = pc;
    int bestDist = 128;

    for (int step : scaleIntervals)
    {
        const int allowed = (rootPC + step) % 12;
        int d = std::abs(allowed - pc);
        d = std::min(d, 12 - d);
        if (d < bestDist)
        {
            bestDist = d;
            bestPc = allowed;
            if (bestDist == 0) break;
        }
    }

    const int base = in - pc; // keep octave, replace only pitch class
    return juce::jlimit(0, 127, base + bestPc);
}

static void applyScaleLockIfNeeded(std::vector<Note>& notes,
    int rootPC,
    const std::vector<int>& scaleIntervals,
    const AdvancedHarmonyOptions* adv)
{
    if (advancedAllowsChromatic(adv))
        return; // Advanced harmony active: allow chromatic tones.

    for (auto& n : notes)
        n.pitch = snapToScalePC(n.pitch, rootPC, scaleIntervals);
}

// Degree (0..N) in current scale -> semitone offset inside that scale (wraps safely)
static int degreeToScaleSemis(int degree, const std::vector<int>& scaleIntervals)
{
    const int n = (int)scaleIntervals.size();
    if (n == 0) return 0;

    // Allow degrees beyond [0..n-1] and negatives; wrap and carry octaves.
    int octaveCarry = degree / n;  // truncates toward 0
    int idx = degree % n;

    // Fix negative modulo so that -1 maps to last scale step and adjusts octave
    if (idx < 0)
    {
        idx += n;
        --octaveCarry;
    }

    return scaleIntervals[idx] + 12 * octaveCarry;
}

std::vector<Note> MidiGenerator::generateMelody()
{
    std::vector<Note> out;

    const int bars = getBars();
    const int tsNum = getTSNumerator();
    const int tsDen = getTSDenominator(); juce::ignoreUnused(tsDen);
    const int rootPC = getKeySemitone();
    const int scaleI = getScaleIndex();

    const auto& sc = getScaleByIndex(scaleI).intervals;

    // --- Pick a predominant note length mode for this generation (random 1 of 4) ---
    {
        auto& sysRng = juce::Random::getSystemRandom();
        const int pick = sysRng.nextInt({ 0, 3 }); // 0..2
        switch (pick)
        {
        case 0: predominantLen_ = PredLen::Sixteenth; break;
        case 1: predominantLen_ = PredLen::Eighth;    break;
        case 2: default: predominantLen_ = PredLen::Quarter; break;
        }
    }


    // Humanize & density knobs already exposed by getters
    const float dens = juce::jlimit(0.0f, 1.0f, getRestDensity()); // 0..1 rests; invert for note prob
    const float noteP = juce::jlimit(0.05f, 0.95f, 1.0f - dens);    // probability to place a note
    const float notePMode = (predominantLen_ == PredLen::Quarter) ? (noteP * 0.75f) : noteP;
    const float tJit = 0.02f * getHumanizeTiming();                // timing jitter in beats fraction
    const float lenJit = 0.25f * getFeelAmount();                    // length jitter fraction

    // simple 8th-note grid melody
    const int stepsPerBeat = (predominantLen_ == PredLen::Sixteenth) ? 4 : 2;
    const float stepBeats = 1.0f / (float)stepsPerBeat;

    // ==== KEY FIX: melody tessitura aligned with chords (base octave = 3) ====
    // chords engine centers around octave 3; match that here so melody isn’t an octave higher
    const int low = 48 + rootPC - 12; // around C3..C4
    const int high = 48 + rootPC + 12; // around C3..C5

    double beat = 0.0;
    juce::Random r;

    // Prevent starting a new note while a previous one is still sounding.
// This is what actually lets quarter-notes survive on an 1/8 grid.
    double lastNoteEnd = -1.0;
    const double minGap = 0.02; // small gap so nothing re-overlaps

    // --- Choose melodic note length with dotted & triplet support ---
    auto pickMelodyLenBeats = [&](juce::Random& rng) -> double
    {
        // Base weights (un-normalized). We include a dedicated 1/16 option now.
        // Straight lengths:
        double wSixteenth = 0.10; // 1/16 = 0.25 beats
        double wEighth = 0.45; // 1/8  = 0.50 beats
        double wQuarter = 0.35; // 1/4  = 1.00 beats
        double wHalf = 0.10; // 1/2  = 2.00 beats

        // Dotted/triplet variants (kept, still available but generally lighter):
        double wDot8 = allowDotted_ ? 0.18 : 0.00; // 0.75
        double wDot4 = allowDotted_ ? 0.12 : 0.00; // 1.50
        double wDot2 = allowDotted_ ? 0.05 : 0.00; // 3.00
        double wTri8 = allowTriplets_ ? 0.15 : 0.00; // 1/3
        double wTri4 = allowTriplets_ ? 0.12 : 0.00; // 2/3
        double wTri2 = allowTriplets_ ? 0.06 : 0.00; // 4/3

        // --- Heavily bias toward the chosen predominant length mode ---
        auto boost = [&](double& target, double factor) { target *= factor; };
        switch (predominantLen_)
        {
        case PredLen::Sixteenth:
            boost(wSixteenth, 3.0); // make 16ths most frequent
            // de-emphasize very long notes a bit so the feel is energetic
            wHalf *= 0.6;
            break;
        case PredLen::Eighth:
            boost(wEighth, 2.5);
            break;
        case PredLen::Quarter:
            boost(wQuarter, 2.5);
            // de-emphasize tiny notes so quarters dominate
            wSixteenth *= 0.6;
            break;
        case PredLen::Half:
            boost(wHalf, 3.0);
            // trim down fast stuff more so the long feel wins
            wSixteenth *= 0.5;
            wEighth *= 0.7;
            break;
        }

        struct Opt { double len; double w; };
        std::array<Opt, 13> opts {{
            { 0.25, wSixteenth }, { 0.50, wEighth }, { 1.00, wQuarter }, { 2.00, wHalf },
            { 0.75, wDot8 }, { 1.50, wDot4 }, { 3.00, wDot2 },
            { 1.0 / 3.0, wTri8 }, { 2.0 / 3.0, wTri4 }, { 4.0 / 3.0, wTri2 },
                // a couple of useful in-betweens for realism (only lightly weighted)
            { 0.375, (allowDotted_ ? 0.06 : 0.0) },  // dotted 16th-ish
            { 0.875, (allowDotted_ ? 0.04 : 0.0) },  // near dotted 8th feel
            { 1.25,  (allowDotted_ ? 0.03 : 0.0) }   // long-ish phrasing
            }};

        double total = 0.0;
        for (const auto& o : opts) total += o.w;
        if (total <= 0.0) return 0.5; // safety: default to eighth

        const double r01 = rng.nextDouble() * total;
        double acc = 0.0;
        for (const auto& o : opts) { acc += o.w; if (r01 <= acc) return o.len; }
        return 0.5;
    };

    auto pickScalePitchDirect = [&](int octaveBias)->int
    {
        const int deg = r.nextInt(juce::jmax(1, (int)sc.size()));
        const int pc = (rootPC + sc[(size_t)deg]) % 12;

        // ==== KEY FIX: use octave 3 as baseline instead of 4 ====
        const int oc = 3 + octaveBias;

        const int midi = pc + oc * 12;
        return juce::jlimit(low, high, midi);
    };

    for (int b = 0; b < bars; ++b)
    {
        for (int s = 0; s < tsNum * stepsPerBeat; ++s)
        {
            // If a previous note is still sounding, skip placing a new one at this grid tick
            if (beat < (lastNoteEnd - minGap))
            {
                beat += stepBeats;
                continue;
            }

            if (r.nextFloat() <= notePMode)
            {
                const float startJ = juce::jmap((float)r.nextDouble(), -tJit, tJit);
                const float lenMul = 0.9f + juce::jmap((float)r.nextDouble(), -lenJit, lenJit);

                Note n;
                // choose nearby octave up/down to add contour variance
                n.pitch = pickScalePitchDirect(r.nextInt(2) - 1);
                n.startBeats = beat + startJ;

                // --- Pick length from straight/dotted/triplet palette ---
                const double baseLen = pickMelodyLenBeats(r);
                n.lengthBeats = std::max(0.125, baseLen * (double)lenMul);

                // velocity & flags
                n.velocity = juce::jlimit(1, 127, (int)juce::jmap((float)r.nextDouble(), 80.0f, 112.0f));
                n.isOrnament = false;

                out.push_back(n);

                // Hold off new placements until this note ends
                lastNoteEnd = n.startBeats + n.lengthBeats;
            }

            beat += stepBeats;
        }
    }

    // Respect scale lock if advanced harmony doesn’t allow chromatic tones
    applyScaleLockIfNeeded(out, rootPC, sc, advOpts_);

    // (Your file also did a second snap pass; keep it if you want that strictness)
    {
        auto snapPc = [&](int midi) -> int
        {
            const int in = juce::jlimit(0, 127, midi);
            const int pc = ((in % 12) + 12) % 12;

            int bestPc = pc;
            int bestDist = 128;

            for (int step : sc)
            {
                const int allowed = (rootPC + step) % 12;
                int d = std::abs(allowed - pc);
                d = std::min(d, 12 - d);
                if (d < bestDist) { bestDist = d; bestPc = allowed; if (bestDist == 0) break; }
            }

            const int base = in - pc;
            return juce::jlimit(0, 127, base + bestPc);
        };

        for (auto& n : out)
            n.pitch = snapPc(n.pitch);
    }

    // --- Phrase shaping & loop logic (with 5% skip)
    {
        const int bars = getBars();
        const int beatsPerBar = getTSNumerator();
        const int keyPc = getKeySemitone() % 12;

        if (juce::Random::getSystemRandom().nextFloat() >= 0.05f)
            bang_applyLoopingPhrases(out, bars, beatsPerBar, keyPc);

        bang_limitMelodyFastNotes(out, predominantLen_ == PredLen::Sixteenth);
    }

    // --- Enforce monophony BEFORE timing humanize ---
    bang_enforceMonophonic(out);

    // --- Humanization + end shaping (keeps off-beat swing and clears collisions) ---
    bang_applyTimingAndEnds(out,
        tsNum,
        getSwingAmount(),
        getHumanizeTiming(),
        getHumanizeVelocity(),
        (predominantLen_ == PredLen::Sixteenth ? 0.25 : 0.50));

    bang_finalSanitizeNotes(out,
        (predominantLen_ == PredLen::Sixteenth ? 0.25 : 0.50),
        true);

    lastOut_ = out;
    return out;
}

// Pick n unique indices from [0..count-1]
static std::vector<int> pickUnique(std::mt19937& rng, int count, int n)
{
    n = std::max(0, std::min(n, count));
    std::vector<int> idx(count);
    std::iota(idx.begin(), idx.end(), 0);
    std::shuffle(idx.begin(), idx.end(), rng);
    idx.resize(n);
    return idx;
}

// Adds chord extensions / sus / alt / slash based on advOpts_->extensionDensity01
// 'triad' are the chord notes (MIDI) for one chord at a given beat
// The function mutates 'triad' in place (adds or modifies top notes).
void MidiGenerator::applyExtensionsAndOthers(std::vector<Note>& triad, int chordRootMidi)
{
    if (!advOpts_) return;

    auto chance = [&](float p01) -> bool
    {
        if (p01 <= 0.0f) return false;
        std::uniform_real_distribution<float> d(0.0f, 1.0f);
        return d(rng) < p01;
    };

    const float d = juce::jlimit(0.0f, 1.0f, advOpts_->extensionDensity01);

    // Decide once per chord if we’ll decorate it at all
    if (!chance(d)) return;

    // Helper to add a chord tone above existing top
    auto addTone = [&](int semitoneOffsetFromRoot, int vel = 90)
    {
        int topStart = triad.empty() ? 0 : (int)triad.front().startBeats;
        int topLen = triad.empty() ? 1 : (int)triad.front().lengthBeats;

        Note n;
        n.pitch = chordRootMidi + semitoneOffsetFromRoot;
        n.velocity = juce::jlimit(1, 127, vel);
        n.startBeats = triad.empty() ? 0.0 : triad.front().startBeats;
        n.lengthBeats = triad.empty() ? 1.0 : triad.front().lengthBeats;
        n.isOrnament = false;

        // keep top within typical range
        n.pitch = juce::jlimit(24, 108, n.pitch);
        triad.push_back(n);
    };

    // SUS handling: replace 3rd with 2 or 4
    auto makeSus = [&](bool sus2)
    {
        // find a 3rd (±3 or ±4 from root, depending on chord quality)
        // simplistic: remove any note 3 or 4 semitones above root in this voicing
        triad.erase(std::remove_if(triad.begin(), triad.end(), [&](const Note& n)
            {
                int rel = (n.pitch - chordRootMidi) % 12;
                if (rel < 0) rel += 12;
                return (rel == 3 || rel == 4); // minor/major 3rd
            }), triad.end());

        int susInt = sus2 ? 2 : 5; // sus2 or sus4 (2 or 5 semitones above root)
        addTone(susInt);
    };

    // ALT handling: very simplified — raise or lower 5th or 9th
    auto makeAlt = [&]()
    {
        // randomly choose one alteration: b5/#5/b9/#9
        std::uniform_int_distribution<int> dAlt(0, 3);
        switch (dAlt(rng))
        {
        case 0: addTone(6);  break; // b5
        case 1: addTone(8);  break; // #5
        case 2: addTone(13); break; // b9 (root + 13 semitones)
        case 3: addTone(15); break; // #9
        }
    };

    // Slash handling: ensure bass note = some chord tone other than root
    auto makeSlash = [&]()
    {
        if (triad.empty()) return;
        // Move one note down an octave to become a bass inversion
        // pick an existing chord tone and drop an octave
        std::uniform_int_distribution<int> di(0, (int)triad.size() - 1);
        int i = di(rng);
        triad[i].pitch = juce::jlimit(0, 127, triad[i].pitch - 12);
    };

    // Extensions: choose which extensions to add
    struct Ext { bool enabled; int semis; };
    std::vector<Ext> exts;
    if (advOpts_->enableExt7)  exts.push_back({ true, 10 });  // minor 7 above root (fits most)
    if (advOpts_->enableExt9)  exts.push_back({ true, 14 });
    if (advOpts_->enableExt11) exts.push_back({ true, 17 });
    if (advOpts_->enableExt13) exts.push_back({ true, 21 });

    if (!exts.empty())
    {
        // pick 1–2 extensions based on density
        int howMany = (d > 0.66f ? 2 : 1);
        howMany = std::min(howMany, (int)exts.size());
        auto idxs = pickUnique(rng, (int)exts.size(), howMany);
        for (int idx : idxs)
            addTone(exts[idx].semis);
    }

    if (advOpts_->enableSus24)
    {
        // pick sus2 or sus4 half the time if we didn’t already change third
        if (chance(0.5f)) makeSus(true); else makeSus(false);
    }

    if (advOpts_->enableAltChords)
    {
        makeAlt();
    }

    if (advOpts_->enableSlashChords)
    {
        makeSlash();
    }
}

// Apply advanced chord *families* across the whole progression, by changing
// selected chord slots. 'progression' is a vector of per-chord note-sets.
void MidiGenerator::applyAdvancedChordFamilies(std::vector<std::vector<Note>>& progression,
    const std::vector<int>& chordRootsMidi)
{
    if (!advOpts_ || progression.empty()) return;

    // Build list of selected families
    enum Fam { SecDom, Borrowed, ChromMed, Neapolitan, TritoneSub };
    std::vector<Fam> chosen;
    if (advOpts_->enableSecondaryDominants) chosen.push_back(SecDom);
    if (advOpts_->enableBorrowed)           chosen.push_back(Borrowed);
    if (advOpts_->enableChromaticMediants)  chosen.push_back(ChromMed);
    if (advOpts_->enableNeapolitan)         chosen.push_back(Neapolitan);
    if (advOpts_->enableTritoneSub)         chosen.push_back(TritoneSub);

    if (chosen.empty()) return;

    // If 1–2 selected → do all of them (one each). If 3–5 → pick exactly 2.
    if ((int)chosen.size() >= 3)
        chosen = [this, &chosen]() {
        std::shuffle(chosen.begin(), chosen.end(), rng);
        chosen.resize(2);
        return chosen;
    }();

    // pick chord indices to modify (one slot per family), all unique
    const int barsToUse = (int)progression.size();
    const int need = (int)chosen.size();
    auto which = pickUnique(rng, barsToUse, need);

    auto applyFam = [&](Fam f, int slot)
    {
        // very simple reharm operations — swap/replace the notes at slot
        // keeping timing/length. We only change pitches.
        auto& notes = progression[slot];
        int root = chordRootsMidi[slot];

        auto transposeAll = [&](int semis)
        {
            for (auto& n : notes) n.pitch = juce::jlimit(0, 127, n.pitch + semis);
        };

        switch (f)
        {
        case SecDom:
            // +7 semis (V of ... feel) → crude but musical enough for now
            transposeAll(7);
            break;
        case Borrowed:
            // minor tint: lower the 3rd if present; add b7
            for (auto& n : notes)
            {
                int rel = (n.pitch - root) % 12; if (rel < 0) rel += 12;
                if (rel == 4) n.pitch -= 1; // maj3 -> min3
            }
            // add a b7 (10 above root)
            notes.push_back(Note{ juce::jlimit(0,127,root + 10), 90, notes.front().startBeats, notes.front().lengthBeats, false });
            break;
        case ChromMed:
            // chromatic mediant feel: +/-4 semis
            transposeAll((rng() & 1) ? 4 : -4);
            break;
        case Neapolitan:
            // bII feel: shift root + all chord tones up a semitone or two to mimic
            transposeAll(1);
            break;
        case TritoneSub:
            // tritone away: -6 or +6
            transposeAll((rng() & 1) ? 6 : -6);
            break;
        }
    };

    for (int i = 0; i < (int)chosen.size(); ++i)
        applyFam(chosen[i], which[i]);
}


std::vector<Note> MidiGenerator::generateChords()
{
    std::vector<Note> out;

    // ===== Read state (existing getters) =====
    const int bars = getBars();
    const int tsNum = getTSNumerator();
    const int tsDen = getTSDenominator(); juce::ignoreUnused(tsDen);
    const bool superBusyTS = (tsDen >= 16) || (tsDen == 8 && tsNum >= 5);
    const int rootPC = getKeySemitone();          // 0..11
    const int scaleI = getScaleIndex();
    const auto& sc = getScaleByIndex(scaleI).intervals;
    const int S = (int)sc.size();

    const float timeHuman = getHumanizeTiming();
    const float velHuman = getHumanizeVelocity();
    const float swingAmt = getSwingAmount();
    const float feelAmt = getFeelAmount();
    const float rest01 = getRestDensity();

    // ===== Small helpers (local) =====
    auto toMidi = [](int pc, int octave) { return juce::jlimit(0, 127, pc + octave * 12); };

    auto pushNote = [&](int pitch, double startBeat, double lenBeats, int vel)
    {
        Note n;
        n.pitch = juce::jlimit(0, 127, pitch);
        n.startBeats = startBeat;
        n.lengthBeats = juce::jmax(0.125, lenBeats);
        n.velocity = juce::jlimit(1, 127, vel);
        out.push_back(n);
    };

    auto swingOffset = [&](double localBeat)->double
    {
        // 8th swing: offset odd 8ths slightly
        const int eighth = int(std::floor(localBeat * 2.0 + 1e-6));
        return ((eighth & 1) ? (0.08 * swingAmt) : 0.0);
    };

    // Diatonic triad (NO 7ths here; Advanced Harmony hook will add later)
    auto triadPCs = [&](int degree)->std::array<int, 3>
    {
        const int d0 = (degree % S + S) % S;
        const int d2 = (d0 + 2) % S;
        const int d4 = (d0 + 4) % S;

        const int pc0 = (rootPC + sc[(size_t)d0]) % 12;
        const int pc1 = (rootPC + sc[(size_t)d2]) % 12;
        const int pc2 = (rootPC + sc[(size_t)d4]) % 12;
        return { pc0, pc1, pc2 };
    };



    // ===== Big seed bank (degrees modulo S) =====
    // These are seeds; we’ll mutate/expand them for variety.
    struct Prog { std::vector<int> deg; float w; };
    const std::vector<Prog> bank = {
        // ===== BIG SEED BANK (diatonic 0..6; allow repeats up to 3x) =====
        // 0=I, 1=ii, 2=iii, 3=IV, 4=V, 5=vi, 6=vii°
        {{0,4,5,3}, 1.00f}, // I–V–vi–IV
        {{0,5,3,4}, 0.98f}, // I–vi–IV–V
        {{1,4,0,0}, 0.97f}, // ii–V–I–I
        {{0,3,4,3}, 0.96f}, // I–IV–V–IV
        {{5,3,0,4}, 0.95f}, // vi–IV–I–V
        {{0,1,4,0}, 0.94f}, // I–ii–V–I
        {{0,3,5,4}, 0.93f}, // I–IV–vi–V
        {{0,4,0,5}, 0.92f}, // I–V–I–vi
        {{0,3,0,4}, 0.92f}, // I–IV–I–V
        {{0,0,4,5}, 0.90f}, // I–I–V–vi
        {{4,3,0,4}, 0.89f}, // V–IV–I–V
        {{1,4,0},   0.89f}, // ii–V–I
        {{4,0},     0.88f}, // V–I (authentic cadence)
        {{4,5},     0.86f}, // V–vi (deceptive cadence)
        {{3,0},     0.85f}, // IV–I (plagal)
        {{0,4},     0.84f}, // I–V
        {{0,3},     0.83f}, // I–IV
        {{5,3},     0.82f}, // vi–IV
        {{0,5},     0.81f}, // I–vi
        {{1,4},     0.80f}, // ii–V
        {{3,4},     0.79f}, // IV–V
        {{2,5},     0.77f}, // iii–vi
        {{6,4},     0.75f}, // vii°–V (leading to V)
        {{6,0},     0.74f}, // vii°–I (strong resolve)
        {{0,6},     0.73f}, // I–vii°
        {{0,2,5,3}, 0.73f}, // I–iii–vi–IV
        {{0,1,2,3,4}, 0.72f}, // I–ii–iii–IV–V
        {{0,6,5,4,3}, 0.71f}, // I–vii°–vi–V–IV
        {{6,2,5,1,4,0}, 0.71f}, // vii°–iii–vi–ii–V–I (circle chain)
        {{0,3,6,2,5,1,4,0}, 0.70f}, // I–IV–vii°–iii–vi–ii–V–I

        // ---- Cadence & turnaround families (with repeats) ----
        {{1,4,0,0}, 0.90f}, // ii–V–I–I
        {{1,4,4,0}, 0.86f}, // ii–V–V–I
        {{1,1,4,0}, 0.84f}, // ii–ii–V–I
        {{4,4,0,0}, 0.83f}, // V–V–I–I
        {{4,0,0,0}, 0.82f}, // V–I–I–I
        {{4,4,0},   0.80f}, // V–V–I
        {{3,4,0,0}, 0.80f}, // IV–V–I–I
        {{3,4,4,0}, 0.78f}, // IV–V–V–I
        {{0,0,4,0}, 0.76f}, // I–I–V–I
        {{0,4,4,0}, 0.75f}, // I–V–V–I
        {{0,4,0,0}, 0.74f}, // I–V–I–I
        {{4,0,4,0}, 0.73f}, // V–I–V–I

        // ---- Two-chord vamps (and holds) ----
        {{0,0},     0.60f}, // I–I
        {{0,0,0},   0.55f}, // I–I–I
        {{3,3},     0.58f}, // IV–IV
        {{3,3,3},   0.54f}, // IV–IV–IV
        {{4,4},     0.58f}, // V–V
        {{4,4,4},   0.54f}, // V–V–V
        {{5,5},     0.56f}, // vi–vi
        {{5,5,5},   0.52f}, // vi–vi–vi
        {{1,1},     0.52f}, // ii–ii
        {{1,1,1},   0.48f}, // ii–ii–ii
        {{2,2},     0.50f}, // iii–iii
        {{2,2,2},   0.46f}, // iii–iii–iii
        {{6,6},     0.48f}, // vii°–vii°
        {{6,6,6},   0.44f}, // vii°–vii°–vii°
        {{0,3,0,3}, 0.66f}, // I–IV–I–IV
        {{0,4,0,4}, 0.66f}, // I–V–I–V
        {{3,4,3,4}, 0.64f}, // IV–V–IV–V
        {{5,3,5,3}, 0.62f}, // vi–IV–vi–IV

        // ---- Three-chord loops (pop/rock staples + repeats) ----
        {{0,4,5},   0.90f}, // I–V–vi
        {{0,5,4},   0.88f}, // I–vi–V
        {{0,3,4},   0.88f}, // I–IV–V
        {{1,4,5},   0.82f}, // ii–V–vi
        {{5,4,0},   0.80f}, // vi–V–I
        {{0,2,5},   0.78f}, // I–iii–vi
        {{0,1,4},   0.78f}, // I–ii–V
        {{3,0,4},   0.77f}, // IV–I–V
        {{4,5,3},   0.76f}, // V–vi–IV
        {{0,4,4},   0.74f}, // I–V–V
        {{0,0,4},   0.73f}, // I–I–V
        {{0,3,3},   0.73f}, // I–IV–IV
        {{5,5,4},   0.72f}, // vi–vi–V
        {{3,4,4},   0.72f}, // IV–V–V
        {{1,1,4},   0.70f}, // ii–ii–V
        {{2,2,5},   0.68f}, // iii–iii–vi
        {{6,4,0},   0.66f}, // vii°–V–I
        {{6,6,0},   0.62f}, // vii°–vii°–I

        // ---- Four-chord—classic & expanded with repeats ----
        {{0,4,5,3}, 1.00f}, // I–V–vi–IV (axis)
        {{0,5,3,4}, 0.98f}, // I–vi–IV–V
        {{5,4,3,0}, 0.93f}, // vi–V–IV–I
        {{0,3,0,4}, 0.92f}, // I–IV–I–V
        {{0,4,0,5}, 0.90f}, // I–V–I–vi
        {{0,0,4,5}, 0.88f}, // I–I–V–vi
        {{0,3,4,4}, 0.86f}, // I–IV–V–V
        {{0,4,4,5}, 0.84f}, // I–V–V–vi
        {{0,0,3,4}, 0.83f}, // I–I–IV–V
        {{0,4,5,5}, 0.82f}, // I–V–vi–vi
        {{5,5,3,4}, 0.80f}, // vi–vi–IV–V
        {{3,3,4,0}, 0.79f}, // IV–IV–V–I
        {{1,4,0,5}, 0.78f}, // ii–V–I–vi
        {{1,4,5,3}, 0.77f}, // ii–V–vi–IV
        {{2,5,3,4}, 0.75f}, // iii–vi–IV–V
        {{0,2,5,4}, 0.74f}, // I–iii–vi–V
        {{0,1,4,5}, 0.73f}, // I–ii–V–vi
        {{3,0,5,4}, 0.72f}, // IV–I–vi–V
        {{4,3,0,0}, 0.72f}, // V–IV–I–I
        {{4,0,0,0}, 0.70f}, // V–I–I–I
        {{0,0,0,4}, 0.68f}, // I–I–I–V
        {{0,3,3,0}, 0.66f}, // I–IV–IV–I
        {{0,5,5,4}, 0.66f}, // I–vi–vi–V
        {{5,3,3,4}, 0.65f}, // vi–IV–IV–V
        {{0,2,2,5}, 0.64f}, // I–iii–iii–vi
        {{1,1,4,0}, 0.64f}, // ii–ii–V–I
        {{6,4,0,0}, 0.63f}, // vii°–V–I–I
        {{6,6,4,0}, 0.61f}, // vii°–vii°–V–I

        // ---- Five-to-eight chord cycles (diatonic circle & friends) ----
        {{0,5,2,6},       0.70f}, // I–vi–iii–vii°
        {{0,5,1,4},       0.72f}, // I–vi–ii–V
        {{0,3,6,2,5},     0.70f}, // I–IV–vii°–iii–vi
        {{0,3,6,2,5,1,4}, 0.69f}, // I–IV–vii°–iii–vi–ii–V
        {{6,2,5,1,4},     0.69f}, // vii°–iii–vi–ii–V
        {{0,1,4,3,0},     0.68f}, // I–ii–V–IV–I
        {{0,4,3,2,1,0},   0.66f}, // I–V–IV–iii–ii–I
        {{0,3,4,0,5,4},   0.64f}, // I–IV–V–I–vi–V
        {{0,4,5,3,0,4},   0.64f}, // I–V–vi–IV–I–V
        {{0,5,4,3,2,1,0}, 0.62f}, // I–vi–V–IV–iii–ii–I

        // ---- One-bar “holds” (all diatonic) ----
        {{0}, 0.50f}, // I
        {{1}, 0.44f}, // ii
        {{2}, 0.42f}, // iii
        {{3}, 0.46f}, // IV
        {{4}, 0.48f}, // V
        {{5}, 0.45f}, // vi
        {{6}, 0.40f}, // vii°

        // ---- Single with explicit triple-repeat (for long pads) ----
        {{0,0,0}, 0.52f}, // I–I–I
        {{3,3,3}, 0.49f}, // IV–IV–IV
        {{4,4,4}, 0.49f}, // V–V–V
        {{5,5,5}, 0.47f}, // vi–vi–vi
        {{1,1,1}, 0.45f}, // ii–ii–ii
        {{2,2,2}, 0.43f}, // iii–iii–iii
        {{6,6,6}, 0.41f}, // vii°–vii°–vii°

        // ---- “Neighbor echo” shapes (stay/step) ----
        {{0,0,3}, 0.66f}, // I–I–IV
        {{0,0,4}, 0.66f}, // I–I–V
        {{3,3,4}, 0.64f}, // IV–IV–V
        {{4,4,0}, 0.64f}, // V–V–I
        {{5,5,3}, 0.62f}, // vi–vi–IV
        {{1,1,4}, 0.60f}, // ii–ii–V
        {{2,2,5}, 0.58f}, // iii–iii–vi

        // ---- Add more four-chord patterns with intentional holds ----
        {{0,0,3,4}, 0.72f}, // I–I–IV–V
        {{0,3,4,4}, 0.72f}, // I–IV–V–V
        {{0,4,5,5}, 0.71f}, // I–V–vi–vi
        {{5,3,3,0}, 0.70f}, // vi–IV–IV–I
        {{4,4,0,5}, 0.68f}, // V–V–I–vi
        {{3,3,0,4}, 0.68f}, // IV–IV–I–V
        {{0,5,5,3}, 0.66f}, // I–vi–vi–IV
        {{0,2,2,5}, 0.64f}, // I–iii–iii–vi
        {{1,4,4,0}, 0.64f}, // ii–V–V–I
        {{6,4,4,0}, 0.62f}, // vii°–V–V–I

        // ---- Half-cadence landings (end on V) ----
        {{0,3,4},   0.80f}, // I–IV–V
        {{0,5,4},   0.78f}, // I–vi–V
        {{1,2,4},   0.74f}, // ii–iii–V
        {{5,1,4},   0.72f}, // vi–ii–V
        {{3,2,4},   0.72f}, // IV–iii–V
        {{0,0,4},   0.70f}, // I–I–V
        {{3,3,4},   0.68f}, // IV–IV–V
        {{5,5,4},   0.66f}, // vi–vi–V

        // ---- Deceptive cadence landings (to vi) ----
        {{4,5},     0.86f}, // V–vi
        {{1,4,5},   0.82f}, // ii–V–vi
        {{0,4,5},   0.80f}, // I–V–vi
        {{3,4,5},   0.78f}, // IV–V–vi
        {{2,4,5},   0.74f}, // iii–V–vi

        // ---- Plagal gestures (IV→I) with setups ----
        {{3,0},     0.85f}, // IV–I
        {{5,3,0},   0.80f}, // vi–IV–I
        {{1,3,0},   0.76f}, // ii–IV–I
        {{2,3,0},   0.74f}, // iii–IV–I
        {{4,3,0},   0.72f}, // V–IV–I

        // ---- Tonic prolongation with passing chords ----
        {{0,2,1,0}, 0.72f}, // I–iii–ii–I
        {{0,1,2,0}, 0.72f}, // I–ii–iii–I
        {{0,5,4,0}, 0.71f}, // I–vi–V–I
        {{0,3,2,1,0}, 0.70f}, // I–IV–iii–ii–I

        // ---- Mixed “axis” variants (reshape order, keep diatonic) ----
        {{5,4,0,3}, 0.86f}, // vi–V–I–IV
        {{3,0,5,4}, 0.84f}, // IV–I–vi–V
        {{4,5,3,0}, 0.84f}, // V–vi–IV–I
        {{0,4,3,5}, 0.82f}, // I–V–IV–vi
        {{0,5,4,3}, 0.82f}, // I–vi–V–IV
        {{5,3,4,0}, 0.80f}, // vi–IV–V–I

        // ---- Minor “color” inside diatonic (still 0..6 degrees) ----
        {{5,1,4,0}, 0.78f}, // vi–ii–V–I
        {{2,5,1,4}, 0.74f}, // iii–vi–ii–V
        {{6,1,4,0}, 0.72f}, // vii°–ii–V–I

        // ---- Longer arcs ending I or V ----
        {{0,1,4,5,3,0}, 0.70f}, // I–ii–V–vi–IV–I
        {{0,2,5,1,4},   0.69f}, // I–iii–vi–ii–V
        {{0,3,6,2,5,4}, 0.67f}, // I–IV–vii°–iii–vi–V
        {{0,5,1,4,0},   0.67f}, // I–vi–ii–V–I
        {{0,3,0,4,5},   0.66f}, // I–IV–I–V–vi

        // ---- Symmetry / “pedal” style patterns ----
        {{0,4,0,4}, 0.70f}, // I–V–I–V
        {{0,3,0,3}, 0.70f}, // I–IV–I–IV
        {{0,5,0,5}, 0.66f}, // I–vi–I–vi
        {{4,0,4,0}, 0.66f}, // V–I–V–I
        {{3,0,3,0}, 0.64f}, // IV–I–IV–I

        // ---- “Question/answer” phrase pairs ----
        {{0,3,4,4,  0,5,4,0}, 0.72f}, // (I–IV–V–V) → (I–vi–V–I)
        {{0,4,5,3,  0,1,4,0}, 0.72f}, // (I–V–vi–IV) → (I–ii–V–I)
        {{0,5,3,4,  0,0,4,0}, 0.70f}, // (I–vi–IV–V) → (I–I–V–I)

        // ---- “Hold then move” patterns ----
        {{0,0,3,4}, 0.72f}, // I–I–IV–V
        {{0,0,4,5}, 0.70f}, // I–I–V–vi
        {{3,3,4,0}, 0.69f}, // IV–IV–V–I
        {{4,4,0,5}, 0.68f}, // V–V–I–vi
        {{5,5,3,4}, 0.66f}, // vi–vi–IV–V

        // ---- All diatonic pairs (curated weights) ----
        {{0,1}, 0.62f}, // I–ii
        {{0,2}, 0.60f}, // I–iii
        {{0,3}, 0.83f}, // I–IV
        {{0,4}, 0.84f}, // I–V
        {{0,5}, 0.81f}, // I–vi
        {{0,6}, 0.73f}, // I–vii°
        {{1,0}, 0.60f}, // ii–I
        {{1,2}, 0.58f}, // ii–iii
        {{1,3}, 0.60f}, // ii–IV
        {{1,4}, 0.80f}, // ii–V
        {{1,5}, 0.62f}, // ii–vi
        {{1,6}, 0.56f}, // ii–vii°
        {{2,0}, 0.60f}, // iii–I
        {{2,1}, 0.58f}, // iii–ii
        {{2,3}, 0.60f}, // iii–IV
        {{2,4}, 0.64f}, // iii–V
        {{2,5}, 0.77f}, // iii–vi
        {{2,6}, 0.54f}, // iii–vii°
        {{3,0}, 0.85f}, // IV–I
        {{3,1}, 0.60f}, // IV–ii
        {{3,2}, 0.60f}, // IV–iii
        {{3,4}, 0.79f}, // IV–V
        {{3,5}, 0.66f}, // IV–vi
        {{3,6}, 0.56f}, // IV–vii°
        {{4,0}, 0.88f}, // V–I
        {{4,1}, 0.66f}, // V–ii
        {{4,2}, 0.64f}, // V–iii
        {{4,3}, 0.70f}, // V–IV
        {{4,5}, 0.86f}, // V–vi
        {{4,6}, 0.62f}, // V–vii°
        {{5,0}, 0.80f}, // vi–I
        {{5,1}, 0.66f}, // vi–ii
        {{5,2}, 0.66f}, // vi–iii
        {{5,3}, 0.82f}, // vi–IV
        {{5,4}, 0.78f}, // vi–V
        {{5,6}, 0.58f}, // vi–vii°
        {{6,0}, 0.74f}, // vii°–I
        {{6,1}, 0.60f}, // vii°–ii
        {{6,2}, 0.58f}, // vii°–iii
        {{6,3}, 0.58f}, // vii°–IV
        {{6,4}, 0.75f}, // vii°–V
        {{6,5}, 0.60f}, // vii°–vi

        // ---- Trios covering all diatonic cadential centers (curated) ----
        {{0,1,0}, 0.68f}, // I–ii–I
        {{0,2,0}, 0.66f}, // I–iii–I
        {{0,3,0}, 0.74f}, // I–IV–I
        {{0,4,0}, 0.78f}, // I–V–I
        {{0,5,0}, 0.72f}, // I–vi–I
        {{0,6,0}, 0.66f}, // I–vii°–I
        {{1,4,1}, 0.64f}, // ii–V–ii
        {{3,4,3}, 0.66f}, // IV–V–IV
        {{4,0,4}, 0.70f}, // V–I–V
        {{5,4,5}, 0.66f}, // vi–V–vi
        {{6,4,6}, 0.62f}, // vii°–V–vii°
        {{2,5,2}, 0.62f}, // iii–vi–iii

        // ---- Strong minor-ish motion within major collection (still diatonic) ----
        {{5,1,4,0}, 0.78f}, // vi–ii–V–I
        {{2,5,1,4}, 0.74f}, // iii–vi–ii–V
        {{6,2,5,1}, 0.70f}, // vii°–iii–vi–ii
        {{5,4,1,4,0}, 0.68f}, // vi–V–ii–V–I
        {{2,1,4,0}, 0.66f}, // iii–ii–V–I

        // ---- End banks: safety/variety (rare but musical) ----
        {{6,0,5,4}, 0.60f}, // vii°–I–vi–V
        {{6,4,3,0}, 0.60f}, // vii°–V–IV–I
        {{2,6,5,4}, 0.58f}, // iii–vii°–vi–V
        {{1,6,4,0}, 0.58f}, // ii–vii°–V–I
        {{3,6,2,5}, 0.56f}, // IV–vii°–iii–vi
        {{4,6,1,0}, 0.56f}, // V–vii°–ii–I
    };

    auto chooseProg = [&]() -> const Prog&
    {
        float sum = 0.f; for (auto& p : bank) sum += p.w;
        float t = (float)rand01() * sum;
        for (auto& p : bank) { if ((t -= p.w) <= 0.f) return p; }
        return bank.front();
    };

    // ===== Phrase length chooser (1, 2, 4, 8 bars) =====
    auto choosePhraseBars = [&]() -> int
    {
        struct C { int bars; float w; };
        const C choices[] = { {1, 0.32f}, {2, 0.36f}, {4, 0.24f}, {8, 0.08f} };
        float sum = 0.f; for (auto& c : choices) sum += c.w;
        float t = (float)rand01() * sum;
        for (auto& c : choices) { if ((t -= c.w) <= 0.f) return c.bars; }
        return 4;
    };

    const int phraseBars = choosePhraseBars();
    const int phraseBeats = phraseBars * tsNum;
    const int songBeats = bars * tsNum;

    // ===== Global rhythm segmenter (NOW: prefer 1/2 and whole; very rare 1/4 & 1/8) =====
    std::vector<double> segPalette;  // we’ll fill this below

    // Gate for rare short segments: “every 30 chords, 50% chance to allow quarters/eighths”
    static uint64_t g_bangChordSegPickCounter = 0;
    const bool allowShortsThisWindow = ((++g_bangChordSegPickCounter % 30ull) == 0) && (rand01() < 0.5);

    // Always include whole(4.0) and half(2.0)
    segPalette = { 4.0, 2.0 };

    // Only sometimes also include quarter/eighth (rare)
    if (allowShortsThisWindow)
    {
        segPalette.push_back(1.0);
        segPalette.push_back(0.5);
    }

    // In “super busy” meters, don’t add extra short choices.
    if (superBusyTS)
    {
        // Keep only long values
        segPalette.erase(
            std::remove_if(segPalette.begin(), segPalette.end(),
                [](double v) { return v < 2.0 - 1e-9; }),
            segPalette.end()
        );
        if (segPalette.empty())
            segPalette = { 2.0 }; // safety
    }

    // Strongly favor whole (4.0) and half (2.0); short values are heavily down-weighted
    auto segWeight = [](double v)->float
    {
        if (v >= 3.99)                    return 1.40f; // whole
        if (v >= 1.99 && v <= 2.01)       return 1.00f; // half
        if (v >= 0.99 && v <= 1.01)       return 0.10f; // quarter (rare)
        if (v >= 0.49 && v <= 0.51)       return 0.05f; // eighth  (very rare)
        return 0.01f; // anything else (practically never)
    };

    // Helper: choose a segment from a candidate list with weights
    auto weightedPick = [&](const std::vector<double>& cand)->double
    {
        float sum = 0.f; for (double v : cand) sum += segWeight(v);
        if (sum <= 0.f) return cand.empty() ? 0.0 : cand.front();
        float t = (float)rand01() * sum;
        for (double v : cand) { t -= segWeight(v); if (t <= 0.0f) return v; }
        return cand.front();
    };

    // Standard chooser, *not yet* enforcing the 10% rule
    auto chooseSegLenRaw = [&](double remaining)->double
    {
        std::vector<double> c; c.reserve(segPalette.size());
        for (double v : segPalette) if (v <= remaining + 1e-9) c.push_back(v);
        if (c.empty()) return remaining;
        return weightedPick(c);
    };

    // ===== Progression expansion logic (mutators + Markov-ish walker) =====
    // 1) Start from a chosen seed.
    const Prog& seed1 = chooseProg();
    std::vector<int> base = seed1.deg;

    // 2) Mutators that create repeats/holds/turnarounds without leaving the diatonic space.
    auto mutateRepeatHolds = [&](std::vector<int>& d)
    {
        // Randomly duplicate some entries to create holds / repeated chords.
        // Probability scaled by feel (higher feel -> a bit more motion, fewer long holds).
        const float holdP = juce::jmap(feelAmt, 0.0f, 1.0f, 0.35f, 0.18f);
        std::vector<int> out; out.reserve(d.size() * 2);
        for (size_t i = 0; i < d.size(); ++i)
        {
            out.push_back(d[i]);
            if (rand01() < holdP)
            {
                // repeat current chord 1–2 times
                out.push_back(d[i]);
                if (rand01() < 0.25) out.push_back(d[i]);
            }
        }
        d.swap(out);
    };

    auto mutateNeighborEcho = [&](std::vector<int>& d)
    {
        // “Neighbor echo”: occasionally insert previous degree between two moves (I–IV becomes I–I–IV).
        if (d.size() < 2) return;
        std::vector<int> out; out.reserve(d.size() * 2);
        for (size_t i = 0; i < d.size(); ++i)
        {
            if (i > 0 && rand01() < 0.18)
                out.push_back(d[i - 1]); // echo hold
            out.push_back(d[i]);
        }
        d.swap(out);
    };

    auto mutateTurnarounds = [&](std::vector<int>& d)
    {
        // Add a simple V -> I or V -> vi cadence at some boundaries.
        if (d.empty()) return;
        std::vector<int> out; out.reserve(d.size() + d.size() / 3 + 4);
        for (size_t i = 0; i < d.size(); ++i)
        {
            out.push_back(d[i]);
            if (rand01() < 0.20)
            {
                // Insert a V (degree 4 in our 0-based mapping) before landing
                out.push_back(4 % S);
            }
        }
        d.swap(out);
    };

    mutateRepeatHolds(base);
    mutateNeighborEcho(base);
    mutateTurnarounds(base);

    // 3) Markov-ish walker to keep going smoothly through degrees:
    auto nextDegree = [&](int cur)->int
    {
        // Preferred moves: stay, step ±1, jump to V or IV, occasional vi/ii.
        struct Choice { int off; float w; };
        const Choice choices[] = {
            { 0, 0.60f },  // stay (hold)
            { +1,0.45f },  // step up
            { -1,0.45f },  // step down
            { +4,0.30f },  // up a fourth (to V)
            { -3,0.28f },  // down a fourth (to IV)
            { +5,0.22f },  // up a fifth
            { +2,0.22f },  // to ii area
            { -5,0.18f },  // down a fifth
        };
        float sum = 0.f; for (auto& c : choices) sum += c.w;
        float t = (float)rand01() * sum;
        for (auto& c : choices)
        {
            t -= c.w; if (t <= 0.f) return (cur + c.off) % S;
        }
        return cur;
    };

    // Build a long degree tape from the base (repeating base, mixing in Markov steps).
    std::vector<int> degreeTape;
    degreeTape.reserve((size_t)(songBeats * 2));
    {
        int cur = base.empty() ? 0 : (base[0] % S + S) % S;
        size_t i = 0;
        while (degreeTape.size() < (size_t)(songBeats + 8))
        {
            // Mostly follow mutated base, but sometimes branch to a related degree.
            if (!base.empty() && rand01() < 0.70)
            {
                cur = base[i % base.size()] % S;
                ++i;
            }
            else
            {
                cur = nextDegree(cur);
            }
            if (cur < 0) cur += S;
            degreeTape.push_back(cur % S);
        }
    }

    // ===== Build the global chord schedule (degree + duration) =====
    struct ChSeg { int degree; double dur; };
    std::vector<ChSeg> schedule; schedule.reserve((size_t)(songBeats / 0.5 + 8));

    int songPosBeats = 0;
    size_t tapeIdx = 0;

    int changeCount = 0;
    int shortChangeCount = 0;

    // Target ratio for 8th/16th changes: ~10%
    constexpr double kShortChangeTarget = 0.10;

    while (songPosBeats < songBeats)
    {
        // phrase alignment (optional musical reset)
        if (songPosBeats % phraseBeats == 0 && rand01() < 0.15)
        {
            // slight chance to “recenter” on tonic at phrase boundaries
            degreeTape[tapeIdx % degreeTape.size()] = 0 % S;
        }

        double remaining = std::min<double>(phraseBeats - (songPosBeats % phraseBeats),
            songBeats - songPosBeats);

        while (remaining > 1e-6)
        {
            double segLen = chooseSegLenRaw(remaining);

            // merge tiny segments to avoid over-choppiness
// Enforce the 10% cap on short changes (<= 0.5 beat)
            auto isShort = [](double d) { return d <= 0.5 + 1e-9; };

            // If we've already used up our short allowance, force a long selection when possible.
            const bool atOrBeyondShortCap =
                (changeCount > 0) && ((double)shortChangeCount / (double)changeCount >= kShortChangeTarget);

            if (isShort(segLen) && atOrBeyondShortCap)
            {
                // Try to pick a non-short segment among remaining choices
                std::vector<double> longCands;
                longCands.reserve(segPalette.size());
                for (double v : segPalette)
                    if (v <= remaining + 1e-9 && !isShort(v))
                        longCands.push_back(v);

                if (!longCands.empty())
                    segLen = weightedPick(longCands);
                // else: we cannot place a long segment (remaining is too small); keep the short one
            }
            ++changeCount;
            if (isShort(segLen)) ++shortChangeCount;

            const int degree = degreeTape[tapeIdx++ % degreeTape.size()];
            schedule.push_back({ degree, segLen });

            remaining -= segLen;
            songPosBeats += (int)std::round(segLen);
        }
    }

    // ===== Render schedule into triads with humanize/swing =====
    const int baseOct = 3;
    double beatCursor = 0.0;

    std::vector<std::vector<Note>> progression;
    std::vector<int>               chordRootsMidi;
    progression.reserve(schedule.size());
    chordRootsMidi.reserve(schedule.size());

    // Keep last chord voicing for voice-leading
    std::array<int, 3> prevVoicing = { toMidi(0,3), toMidi(0,3), toMidi(0,3) }; // harmless init
    bool havePrevVoicing = false;

    for (const auto& cs : schedule)
    {
        // Sparse rests to give air
        if ((cs.dur <= 0.5 && rand01() < rest01 * 0.5) ||
            (cs.dur > 0.5 && rand01() < rest01 * 0.15))
        {
            beatCursor += cs.dur;
            continue;
        }

        const double tJit = juce::jmap((float)rand01(), -0.02f * timeHuman, 0.02f * timeHuman);
        const double lJit = juce::jmap((float)rand01(), -0.15f * feelAmt, 0.12f * feelAmt);
        const double start = beatCursor + tJit + swingOffset(beatCursor);
        const double len = juce::jmax(0.25, cs.dur * (1.0 + lJit));

        const auto pcs = triadPCs(cs.degree);

        const int baseVel = juce::jlimit(50, 120,
            (int)juce::jmap((float)rand01(), 92.0f, 112.0f)
            + (int)juce::roundToInt(velHuman * 6.0f));

        const int p0 = toMidi(pcs[0], baseOct);
        const int p1 = toMidi(pcs[1], baseOct);
        const int p2 = toMidi(pcs[2], baseOct);

        // build the chord triad for this hit
// ----- Build voiced triad with inversions + basic voice leading -----

// root-position triad around baseOct
        std::array<int, 3> rootPos = { toMidi(pcs[0], baseOct), toMidi(pcs[1], baseOct), toMidi(pcs[2], baseOct) };
        std::array<int, 3> firstInv = { rootPos[1],               rootPos[2],               rootPos[0] + 12 };
        std::array<int, 3> secondInv = { rootPos[2],               rootPos[0] + 12,          rootPos[1] + 12 };

        // Evaluate movement cost vs previous voicing (sum of absolute diffs)
        auto costTo = [&](const std::array<int, 3>& cand)->int
        {
            if (!havePrevVoicing) return 0; // first chord gets a free pass
            int c = 0;
            for (int i = 0; i < 3; ++i) c += std::abs(cand[i] - prevVoicing[i]);
            return c;
        };

        struct Cand { std::array<int, 3> v; int cost; };
        std::array<Cand, 3> cand {{
            { rootPos, costTo(rootPos)   },
            { firstInv,  costTo(firstInv) },
            { secondInv, costTo(secondInv) }
            }};

        // 70% minimal motion, 30% random inversion
        std::array<int, 3> chosen = cand[0].v;
        if (rand01() < 0.70f)
        {
            auto best = cand[0];
            if (cand[1].cost < best.cost) best = cand[1];
            if (cand[2].cost < best.cost) best = cand[2];
            chosen = best.v;
        }
        else
        {
            std::uniform_int_distribution<int> di(0, 2);
            chosen = cand[(size_t)di(rng)].v;
        }

        // Declare triad BEFORE any helper lambdas that push into it
        std::vector<Note> triad;
        triad.reserve(5);

        // helper to push a chord tone at the current timing
        auto pushChord = [&](int pitch, int vel)
        {
            Note n;
            n.pitch = juce::jlimit(0, 127, pitch);
            n.velocity = juce::jlimit(1, 127, vel);
            n.startBeats = start;
            n.lengthBeats = len;
            n.isOrnament = false;
            triad.push_back(n);
        };

        // place the three voices (slight top-voice shading)
        pushChord(chosen[0], baseVel);
        pushChord(chosen[1], baseVel - 5);
        pushChord(chosen[2], baseVel - 8);

        // Remember for next chord’s voice-leading
        prevVoicing = chosen;
        havePrevVoicing = true;

        // ----- Bass layer (most chords): root -12, root -24, or root -24 + fifth(-12) -----
        const int chordRootMidi = rootPos[0]; // actual root at baseOct

        if (rand01() < 0.80f) // most chords get bass
        {
            std::uniform_int_distribution<int> bassPick(0, 2);
            const int choice = bassPick(rng);

            auto pushBass = [&](int pitch)
            {
                // Reuse the same timing/len, but a bit softer
                pushChord(pitch, juce::jlimit(1, 127, baseVel - 12));
            };

            switch (choice)
            {
            case 0: // root one octave below
                pushBass(chordRootMidi - 12);
                break;
            case 1: // root two octaves below
                pushBass(chordRootMidi - 24);
                break;
            case 2: // root two octaves below + a fifth in between
                pushBass(chordRootMidi - 24);
                pushBass((chordRootMidi - 12) + 7); // fifth between the two roots
                break;
            }
        }

        // Keep your existing advanced decoration
        applyExtensionsAndOthers(triad, chordRootMidi);

        // Add to the per-chord progression (same containers you already use)
        progression.push_back(triad);
        chordRootsMidi.push_back(chordRootMidi);

        beatCursor += cs.dur;
    }

    // Advanced harmony post-process hook (no-ops if adv opts not set)
// Apply advanced chord families across the whole progression, then flatten.
    applyAdvancedChordFamilies(progression, chordRootsMidi);

    // flatten into 'out'
    out.clear();
    for (auto& chordVec : progression)
    {
        for (auto& n : chordVec)
            out.push_back(n);
    }
    {
        const int rootPC = getKeySemitone();
        const auto& sc = getScaleByIndex(getScaleIndex()).intervals;

        auto snapPc = [&](int midi) -> int
        {
            const int in = juce::jlimit(0, 127, midi);
            const int pc = ((in % 12) + 12) % 12;

            int bestPc = pc;
            int bestDist = 128;

            for (int step : sc)
            {
                const int allowed = (rootPC + step) % 12;
                int d = std::abs(allowed - pc);
                d = std::min(d, 12 - d);
                if (d < bestDist) { bestDist = d; bestPc = allowed; if (bestDist == 0) break; }
            }

            const int base = in - pc;
            return juce::jlimit(0, 127, base + bestPc);
        };

        for (auto& n : out)
            n.pitch = snapPc(n.pitch);
    }

    {
        const int beatsPerBar = getTSNumerator();
        bang_limitChordSubdivisionTo8ths(out, (double)beatsPerBar);
    }

    // --- FINAL GUARD: no chord blips ever (>= 1/8)
   // bang_finalSanitizeNotes(out, 0.50 /* 1/8 beat */, false /* polyphonic */);

    return out;

}


std::vector<Note> MidiGenerator::generateChordTrack()
{
    auto out = generateChords();

    // --- Enforce minimum chord duration = 1/8 note (no 1/16 or 1/32 ever)
    const int beatsPerBar = getTSNumerator();
    bang_limitChordSubdivisionTo8ths(out, (double)beatsPerBar);

    return out;
} 

MidiGenerator::MixBundle MidiGenerator::generateMelodyAndChords(bool avoidOverlaps)
{
    MixBundle outBundle;

    // --- 0) Read state once (use ONLY existing getters/state) ---
    const int bars = getBars();
    const int tsNum = getTSNumerator();
    const int tsDen = getTSDenominator(); juce::ignoreUnused(tsDen);
    const int rootPC = getKeySemitone();         // 0..11
    const int scaleI = getScaleIndex();
    const auto& sc = getScaleByIndex(scaleI).intervals;

    const float timeHuman = getHumanizeTiming();
    const float velHuman = getHumanizeVelocity();
    const float swingAmt = getSwingAmount();
    const float feelAmt = getFeelAmount();
    const float rest01 = getRestDensity();

    const double beatsPerBar = (double)tsNum;

    // --- 1) Make sure we have rhythm patterns (uses your PatternDB) ---
    // If your constructor already builds rhythmDB, this is a no-op; otherwise ensure it's populated.
    // We never depend on file I/O here.
    if (rhythmDB.patterns.empty())
    {
        rhythmDB = makeDefaultRhythms();
    }

    // Helper: pick a rhythm pattern that matches the current meter (by beatsPerBar)
    auto pickPatternForMeter = [&](int beats, int wantBars) -> const RhythmPattern*
    {
        const RhythmPattern* best = nullptr;
        float bestW = -1.0f;

        for (const auto& p : rhythmDB.patterns)
        {
            if (p.beatsPerBar == beats && (wantBars <= 0 || p.bars == wantBars))
            {
                // weight = file weight + light preference for 1–2 bar patterns in mixture
                float w = p.weight + (p.bars <= 2 ? 0.1f : 0.0f);
                if (w > bestW) { bestW = w; best = &p; }
            }
        }

        // Fallback: if nothing matched bars constraint, ignore bars
        if (!best)
        {
            for (const auto& p : rhythmDB.patterns)
                if (p.beatsPerBar == beats && p.weight > bestW)
                {
                    bestW = p.weight; best = &p;
                }
        }
        return best;
    };

    // --- 2) Build a chord progression track (full notes) ---
    // We reuse your existing routine so advanced harmony, cadence shaping, etc., stay intact.
    auto chordTrackFull = generateChords(); // returns Note[] with polyphonic chord chunks over the section

    // Group chord notes by chord onsets (notes that start at the same startBeats are one chord “voicing”)
    struct ChordGroup { double start{}; double len{}; std::vector<Note> notes; };
    std::vector<ChordGroup> chordGroups;

    if (!chordTrackFull.empty())
    {
        std::vector<Note> sorted = chordTrackFull;
        std::sort(sorted.begin(), sorted.end(),
            [](const Note& a, const Note& b) { return a.startBeats < b.startBeats; });

        const double eps = 1e-6;
        ChordGroup cur;
        cur.start = sorted.front().startBeats;
        cur.len = sorted.front().lengthBeats;
        cur.notes.clear();
        for (const auto& n : sorted)
        {
            if (std::abs(n.startBeats - cur.start) <= eps)
            {
                // same chord onset
                cur.notes.push_back(n);
                cur.len = std::max(cur.len, n.lengthBeats);
            }
            else
            {
                chordGroups.push_back(cur);
                cur = {};
                cur.start = n.startBeats;
                cur.len = n.lengthBeats;
                cur.notes = { n };
            }
        }
        chordGroups.push_back(cur);
    }

    // Build a quick look-up: for any beat, what’s the “active” chord group index?
    auto chordIndexAtBeat = [&](double beat)->int
    {
        const double eps = 1e-6;
        for (int i = (int)chordGroups.size() - 1; i >= 0; --i)
        {
            const auto& g = chordGroups[(size_t)i];
            if (beat + eps >= g.start) return i;
        }
        return chordGroups.empty() ? -1 : 0;
    };

    // --- 3) Turn chord groups into short “pieces of chords” on a riff/stab rhythm ---
    std::vector<Note> chordPieces;
    chordPieces.reserve((size_t)bars * 12);

    const RhythmPattern* pat = pickPatternForMeter(tsNum, /*wantBars*/ 1);
    if (!pat)
        pat = pickPatternForMeter(tsNum, /*wantBars*/ 2); // fallback

    // If still nothing, just keep the full chords (mixture will still include melody) — but we prefer pieces:
    bool havePieces = (pat != nullptr);

    if (havePieces)
    {
        // Optional: polyrhythm expansion hook (already provided in your class)
        auto expanded = expandPatternWithPolyrhythm(*pat, /*baseStartBeats*/ 0.0);

        auto mapAccent = [&](float a)->uint8_t {
            return bang_mapAccentToVelocity(juce::jlimit(0.0f, 1.0f, a), 92, 30); // centered ~92, ±15
        };

        // For every bar, stamp the pattern; for each non-rest step, emit a short chord stab
        for (int bar = 0; bar < bars; ++bar)
        {
            const double barStart = (double)bar * beatsPerBar;

            for (const auto& s : expanded)
            {
                if (s.rest) continue; // correct field name

                const double stepStart = barStart + s.startBeats;
                const int cgIdx = chordIndexAtBeat(stepStart);
                if (cgIdx < 0 || cgIdx >= (int)chordGroups.size()) continue;

                const auto& G = chordGroups[(size_t)cgIdx];

                // Use the chord’s voicing, but shorten to the rhythm step (with tiny variance)
                const double rawLen = std::min<double>(s.lengthBeats, std::max(0.25, G.len));
                const double stabLen = std::max(0.25, rawLen * (0.90 + 0.08 * U(rng_)));   // use member RNG
                const double startJ = juce::jmap((float)U(rng_), -0.02f, 0.02f);          // tiny timing jitter

                // copy chord tones as a stab
                for (const auto& tone : G.notes)
                {
                    Note n;
                    n.pitch = tone.pitch;
                    n.velocity = bang_mapAccentToVelocity(juce::jlimit(0.0f, 1.0f, s.accent), 92, 30);
                    n.startBeats = stepStart + startJ;
                    n.lengthBeats = stabLen;
                    n.isOrnament = false;

                    chordPieces.push_back(n);
                }

            }
        }

        // Light clean-up so stabs don’t blur:
        bang_limitChordSubdivisionTo8ths(chordPieces, beatsPerBar);
    }

    // --- 4) Generate melody as usual ---
    auto melody = generateMelody();

    // --- 5) If we failed to find a pattern, fall back to using the original full chords,
    //         but keep them light (shorten/attenuate a bit) so it still feels “mixture-y”.
    if (!havePieces)
    {
        chordPieces.reserve(chordPieces.size() + chordTrackFull.size());
        for (auto n : chordTrackFull)
        {
            // shorten + ease velocity
            n.lengthBeats = std::max(0.50, n.lengthBeats * 0.66);
            n.velocity = juce::jlimit<uint8_t>(1, 127, (uint8_t)juce::roundToInt(n.velocity * 0.85f));
            chordPieces.push_back(n);
        }
    }

    // --- 6) Optional de-mud: trim chord pieces under strong melody onsets ---
    if (avoidOverlaps && !melody.empty() && !chordPieces.empty())
    {
        std::sort(melody.begin(), melody.end(),
            [](const Note& a, const Note& b) { return a.startBeats < b.startBeats; });
        std::sort(chordPieces.begin(), chordPieces.end(),
            [](const Note& a, const Note& b) { return a.startBeats < b.startBeats; });

        const double minGap = 0.02;
        size_t m = 0, c = 0;
        while (m < melody.size() && c < chordPieces.size())
        {
            const auto& mn = melody[m];
            auto& cn = chordPieces[c];

            const double mEnd = mn.startBeats + mn.lengthBeats;
            const double cEnd = cn.startBeats + cn.lengthBeats;

            // If the chord stab overlaps the leading edge of a melody note, duck or trim the stab
            const bool overlaps = !(cEnd <= mn.startBeats + minGap || cn.startBeats >= mEnd - minGap);
            if (overlaps)
            {
                // Prefer trimming the stab to end right before melody starts
                const double allowed = std::max(0.125, (mn.startBeats - minGap) - cn.startBeats);
                if (allowed < cn.lengthBeats)
                    cn.lengthBeats = allowed;

                // If it became too tiny, reduce its velocity so it reads as a “ghost” comp hit
                if (cn.lengthBeats <= 0.16)
                    cn.velocity = juce::jlimit<uint8_t>(1, 127, (uint8_t)juce::roundToInt(cn.velocity * 0.7f));
            }

            // advance the earlier-ending item
            if (mEnd < cEnd) ++m; else ++c;
        }

        // Final guard to remove any non-positive-length crumbs
        chordPieces.erase(std::remove_if(chordPieces.begin(), chordPieces.end(),
            [](const Note& n) { return n.lengthBeats <= 0.0; }),
            chordPieces.end());
    }

    // --- 7) Respect scale lock for chord pieces only if advanced harmony is off ---
    applyScaleLockIfNeeded(chordPieces, rootPC, sc, advOpts_);

    // --- ensure melody retains 1/16 notes when that mode is active ---
    if (!melody.empty())
    {
        bang_applyTimingAndEnds(melody,
            tsNum,
            swingAmt,
            timeHuman,
            velHuman,
            (predominantLen_ == PredLen::Sixteenth ? 0.25 : 0.50)); // 1/16 vs 1/8 floor

        bang_finalSanitizeNotes(melody,
            (predominantLen_ == PredLen::Sixteenth ? 0.25 : 0.50),
            true /* monophonic melody */);
    }

    // chords: keep ≥ 1/8 and polyphonic
    if (!chordPieces.empty())
    {
        bang_finalSanitizeNotes(chordPieces, 0.50, false /* polyphonic chords */);
    }


    // --- 8) Gentle humanization pass on chord pieces (matching your melody humanize) ---
    if (!chordPieces.empty())
    {
        bang_applyTimingAndEnds(chordPieces,
            tsNum,
            swingAmt,
            timeHuman,
            velHuman,
            0.50 /* keep ≥ 1/8 */);

    }


    // --- 9) Output bundle (editor will cache both and display combined in Mixture view) ---
    outBundle.melody = std::move(melody);
    outBundle.chords = std::move(chordPieces);
    lastOut_ = outBundle.melody; // keep legacy field in sync if you use it elsewhere

    return outBundle;
}


