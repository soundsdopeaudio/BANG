#include "MidiGenerator.h"
#include "AdvancedHarmonyWindow.h"
#include <random>
#include <algorithm>
#include <cmath>

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

// ===== Scales table =========================================================
// ===== Scales table =========================================================
static const std::vector<Scale>& kAllScales()
{
    static const std::vector<Scale> scales = {
        {"Major",           {0,2,4,5,7,9,11}},
        {"Natural Minor",   {0,2,3,5,7,8,10}},
        {"Harmonic Minor",  {0,2,3,5,7,8,11}},
        {"Dorian",          {0,2,3,5,7,9,10}},
        {"Phrygian",        {0,1,3,5,7,8,10}},
        {"Lydian",          {0,2,4,6,7,9,11}},
        {"Mixolydian",      {0,2,4,5,7,9,10}},
        {"Aeolian",         {0,2,3,5,7,8,10}},
        {"Locrian",         {0,1,3,5,6,8,10}},

        // ASCII replacements for the “natural” symbol:
        {"Locrian Nat6",    {0,1,3,5,6,9,10}},   // was “Locrian ♮6”
        {"Ionian #5",       {0,2,4,6,7,9,11}},
        {"Dorian #4",       {0,2,3,6,7,9,10}},
        {"Phrygian Dom",    {0,1,3,5,7,9,10}},
        {"Lydian #2",       {0,3,4,6,7,9,11}},
        {"Super Locrian",   {0,1,3,4,6,8,10}},
        {"Dorian b2",       {0,1,3,5,7,9,10}},
        {"Lydian Aug",      {0,2,4,6,8,9,11}},
        {"Lydian Dom",      {0,2,4,6,7,9,10}},
        {"Mixo b6",         {0,2,4,5,7,8,10}},
        {"Locrian #2",      {0,2,3,5,6,8,10}},
        {"Ethiopian Min", {0, 2, 4, 5, 7, 8, 11}},
        {"8 Tone Spanish",  {0,1,3,4,5,6,8,10}},

        {"Phrygian Nat3",   {0,1,4,5,7,8,10}},   // was “Phrygian ♮3”
        {"Blues",           {0,3,5,6,7,10}},
        {"Hungarian Min",   {0,3,5,8,11}},
        {"Harmonic Maj",    {0,2,4,5,7,8,11}},
        {"Pentatonic Maj",  {0,2,5,7,9}},
        {"Pentatonic Min",  {0,3,5,7,10}},
        {"Neopolitan Maj",  {0,1,3,5,7,9,11}},
        {"Neopolitan Min",  {0,1,3,5,7,8,10}},
        {"Spanish Gypsy",   {0,1,4,5,7,8,10}},
        {"Romanian Minor",  {0,2,3,6,7,9,10}},
        {"Chromatic",       {0,1,2,3,4,5,6,7,8,9,10,11}},
        {"Bebop Major",     {0,2,4,5,7,8,9,11}},
        {"Bebop Minor",     {0,2,3,5,7,8,9,10}},
    };
    return scales;
}


const std::vector<Scale>& MidiGenerator::getKnownScales() { return kAllScales(); }

// ===== Basics ===============================================================
MidiGenerator::MidiGenerator()
{
    // default deterministic seed
    setSeed(0);
}

void MidiGenerator::setKey(int semitonesFromC)
{
    keyRoot = juce::jlimit(0, 127, semitonesFromC);
}

void MidiGenerator::setScale(const juce::String& scaleName)
{
    currentScaleName = scaleName;
    // lookup intervals
    for (const auto& s : kAllScales())
        if (s.name == scaleName) { currentScale = s.intervals; return; }
    // fallback: Major if unknown
    currentScale = { 0,2,4,5,7,9,11 };
}

void MidiGenerator::setTimeSignature(int beats, int unit)
{
    tsBeats = juce::jmax(1, beats);
    tsNoteValue = juce::jlimit(1, 32, unit);
}

void MidiGenerator::setBars(int bars)
{
    totalBars = juce::jlimit(1, 128, bars);
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
    if (advOpts == nullptr) return;
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


void MidiGenerator::setPolyrhythmAmount(float amt01) { polyrAmt01_ = juce::jlimit(0.0f, 1.0f, amt01); }

void MidiGenerator::setAdvancedHarmonyOptions(AdvancedHarmonyOptions* opts)
{
    advOpts_ = opts;
}


// ===== Generate stubs (use your existing bodies if you have them) ==========
std::vector<Note> MidiGenerator::generateMelody()
{
    // … your existing melody generation body …
    return {};
}

std::vector<Note> MidiGenerator::generateChordTrack()
{
    // … your existing chord-track generation body …
    return {};
}

MidiGenerator::MixBundle MidiGenerator::generateMelodyAndChords(bool avoidOverlaps)
{
    // 1) Get a fresh melody and a full chord track from your existing generators.
    std::vector<Note> melody = generateMelody();         // must already exist
    std::vector<Note> chords = generateChordTrack();     // must already exist

    // 2) Thin chords into "pieces" (don’t dump every chord tone).
    //    We group by simultaneous starts and randomly keep 1–2 tones per chord hit.
    juce::Random r;
    std::sort(chords.begin(), chords.end(),
        [](const Note& a, const Note& b)
        {
            if (a.startBeats == b.startBeats) return a.pitch < b.pitch;
            return a.startBeats < b.startBeats;
        });

    std::vector<Note> chordPieces;
    chordPieces.reserve(chords.size());

    const double eps = 1.0e-3;
    for (size_t i = 0; i < chords.size();)
    {
        const double t = chords[i].startBeats;

        // collect this chord group (all notes with ~same start)
        std::vector<size_t> group;
        while (i < chords.size() && std::abs(chords[i].startBeats - t) < eps)
        {
            group.push_back(i);
            ++i;
        }

        // randomly keep 1..2 tones (but not more than available)
        int keepCount = juce::jlimit(1, 2, (int)group.size());

        // simple shuffle of indices
        for (int s = (int)group.size() - 1; s > 0; --s)
        {
            int j = r.nextInt(s + 1);
            std::swap(group[(size_t)s], group[(size_t)j]);
        }

        for (int k = 0; k < keepCount; ++k)
            chordPieces.push_back(chords[group[(size_t)k]]);
    }

    // 3) Optionally avoid any overlap between melody and chord pieces.
    if (avoidOverlaps)
    {
        auto overlaps = [](const Note& a, const Note& b)
        {
            const double a1 = a.startBeats;
            const double a2 = a.startBeats + a.lengthBeats;
            const double b1 = b.startBeats;
            const double b2 = b.startBeats + b.lengthBeats;
            return (a1 < b2 && b1 < a2);
        };

        std::vector<Note> cleanedMelody;
        cleanedMelody.reserve(melody.size());

        for (const auto& m : melody)
        {
            bool collide = false;
            for (const auto& c : chordPieces)
            {
                if (overlaps(m, c))
                {
                    collide = true;
                    break;
                }
            }
            if (!collide)
                cleanedMelody.push_back(m);
        }
        melody.swap(cleanedMelody);
    }

    // 4) Return both tracks
    MixBundle out;
    out.melody = std::move(melody);
    out.chords = std::move(chordPieces);
    return out;
}
