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
float MidiGenerator::getRestDensity() const { return restDensity_; }

// ===== Scales catalogue (local static, defined in .cpp to avoid ODR/link issues) =====
const std::vector<MidiGenerator::Scale>& MidiGenerator::kAllScales()
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
    // kAllScales is assumed to be something like:
    // std::map<juce::String, std::vector<int>> kAllScales;
    for (const auto& kv : kAllScales)
    {
        const juce::String key = kv.first;
        if (key.equalsIgnoreCase(name))
        {
            matchedName = key; // preserve canonical spelling from the table
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

std::map<juce::String, std::vector<int>> kAllScales;

void MidiGenerator::setScaleIndex(int idx)
{
    // If you keep a vector/array of ordered scale names for the UI, use that here.
    // If you don't have one, we’ll clamp and then pick by iterating kAllScales.

    const int total = static_cast<int>(kAllScales.size());
    if (total <= 0)
    {
        // defensive: fall back to Major
        scaleIndex_ = 0;
        scaleName_ = "Major";
        return;
    }

    // Clamp index
    const int clamped = juce::jlimit(0, total - 1, idx);

    // Walk to that index in kAllScales (order is whatever you used to fill it)
    int i = 0;
    for (const auto& k : allScales)
    {
        if (i == clamped)
        {
            scaleIndex_ = clamped;
            scaleName_ = kv.first; // store the canonical name
            break;
        }
        ++i;
    }
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
}

void MidiGenerator::setKey(int semitonesFromC)
{
    keyRoot = juce::jlimit(0, 127, semitonesFromC);
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

void MidiGenerator::setScale(const juce::String& name)
{
    juce::String matched;
    if (findScaleByName(name, matched))
    {
        // Set name
        scaleName_ = matched;

        // Also update index to match the name so UI stays in sync
        int i = 0;
        for (const auto& kv : kAllScales)
        {
            if (kv.first == matched)
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

std::vector<Note> MidiGenerator::generateChords()
{
    std::vector<Note> out;

    // Grab state you already store in the generator:
    const int bars = getBars();                 // your existing getter
    const int tsNum = getTSNumerator();         // your existing getter
    const int tsDen = getTSDenominator();       // your existing getter
    const int root = getKeySemitone();         // your existing getter
    const int scaleIdx = getScaleIndex();       // your existing getter

    // Humanize knobs (already added earlier):
    const double timingHuman = getHumanizeTiming();
    const double velHuman = getHumanizeVelocity();
    const double swingAmt = getSwingAmount();
    const double feelAmt = getFeelAmount();

    // Rest density in 0..1 (already wired):
    const float rest = juce::jlimit<float>(0.0f, 1.0f, static_cast<float>(restDensity) * 0.01f);
    const bool makeRest = rand01() < rest;

    const float timingJitter = 0.02f * getHumanizeTiming(); // up to ±2% of a beat for timing
    const float lengthJitter = 0.25f * getFeelAmount(); // up to ±25% of note length

    const float timeJit = juce::jmap<float>(rand01(), -timingJitter, timingJitter);
    const float lenJit = juce::jmap<float>(rand01(), -lengthJitter, lengthJitter);

    // Fetch the scale intervals you already store:
    const Scale& sc = getScaleByIndex(scaleIdx);

    // Chord choices per bar: I–V–vi–IV loop (example baseline),
    // but still respects your rest density & humanization.
    // If you already have a chord progression function, call that instead.
    const int romanToDegree[4] = { 0, 4, 5, 3 }; // I, V, vi, IV (degrees in Ionian)
    const int progressionLen = 4;

    // Simple voxel for note params
    auto pushNote = [&](int pitch, double startBeat, double lenBeats, int vel)
    {
        Note n;
        n.pitch = pitch;
        n.startBeats = startBeat;
        n.lengthBeats = lenBeats;
        n.velocity = juce::jlimit(1, 127, vel);
        out.push_back(n);
    };

    // Helper to quantize swing on 8th notes (optional, mild):
    auto swingOffset = [&](double localBeat)->double
    {
        // 8th grid: odd 8ths get shifted by swingAmt * small fraction of a beat.
        const int eighthIndex = int(std::floor(localBeat * 2.0 + 1e-6));
        if ((eighthIndex & 1) != 0)
            return swingAmt * 0.08; // ~5% of a bar at 4/4; keep subtle
        return 0.0;
    };

    // Iterate all bars
    double globalBeat = 0.0;
    for (int bar = 0; bar < bars; ++bar)
    {
        // Pick chord degree from the loop
        const int deg = romanToDegree[bar % progressionLen];

        // Build a simple triad (root, third, fifth) within the current scale
        const int scaleRoot = (root + sc.intervals[(deg + 0) % sc.intervals.size()]) % 12;
        const int scaleThird = (root + sc.intervals[(deg + 2) % sc.intervals.size()]) % 12;
        const int scaleFifth = (root + sc.intervals[(deg + 4) % sc.intervals.size()]) % 12;

        // Choose octave range you already use; here we anchor around C3..C4:
        auto toMidi = [](int pc, int octave) { return pc + (octave + 1) * 12; };

        // Decide to rest this bar or not based on rest density:
        const bool makeRest = (rand01() < rest);  // use your existing RNG helper

        // Length for the whole bar (in beats)
        const double barBeats = double(tsNum);
        const double start = globalBeat;

        if (!makeRest)
        {
            // Humanized start & length
            const double timeJit = (rand01() - 0.5) * (timingHuman * 0.02);
            const double lenJit = (rand01() - 0.5) * (timingHuman * 0.01);
            const double swung = swingOffset(0.0);
            const int baseVel = int(juce::jmap(rand01(), 80.0, 110.0) + velHuman * 0.2);

            // Add three chord tones
            pushNote(toMidi(scaleRoot, 3), start + timeJit + swung, barBeats + lenJit, baseVel);
            pushNote(toMidi(scaleThird, 3), start + timeJit + swung, barBeats + lenJit, baseVel - 5);
            pushNote(toMidi(scaleFifth, 3), start + timeJit + swung, barBeats + lenJit, baseVel - 8);
        }

        // Advance to next bar
        globalBeat += barBeats;
    }

    return out;
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
