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


// ===== Generate stubs (use your existing bodies if you have them) ==========
std::vector<Note> MidiGenerator::generateMelody()
{
    std::vector<Note> out;

    const int bars = getBars();
    const int tsNum = getTSNumerator();
    const int tsDen = getTSDenominator(); juce::ignoreUnused(tsDen);
    const int rootPC = getKeySemitone();
    const int scaleI = getScaleIndex();

    const auto& sc = getScaleByIndex(scaleI).intervals;

    // Humanize & density knobs already exposed by getters
    const float dens = juce::jlimit(0.0f, 1.0f, getRestDensity()); // 0..1 rests; invert for note prob
    const float noteP = juce::jlimit(0.05f, 0.95f, 1.0f - dens);    // probability to place a note
    const float tJit = 0.02f * getHumanizeTiming();   // timing jitter in beats fraction
    const float lenJit = 0.25f * getFeelAmount();       // length jitter fraction

    // simple 8th-note grid melody
    const int   stepsPerBeat = 2;
    const float stepBeats = 1.0f / (float)stepsPerBeat;

    // range
    const int low = 60 + rootPC - 12; // around C4..C5
    const int high = 60 + rootPC + 12;

    double beat = 0.0;
    juce::Random r;

    auto pickScalePitch = [&](int octaveBias)->int
    {
        const int deg = r.nextInt(juce::jmax(1, (int)sc.size()));
        const int pc = (rootPC + sc[(size_t)deg]) % 12;
        const int oc = 4 + octaveBias;
        return juce::jlimit(low, high, pc + oc * 12);
    };

    for (int b = 0; b < bars; ++b)
    {
        for (int s = 0; s < tsNum * stepsPerBeat; ++s)
        {
            if (r.nextFloat() <= noteP)
            {
                const float startJ = juce::jmap((float)r.nextDouble(), -tJit, tJit);
                const float lenMul = 0.9f + juce::jmap((float)r.nextDouble(), -lenJit, lenJit);

                Note n;
                n.pitch = pickScalePitch(r.nextInt(2) - 1);
                n.startBeats = beat + startJ;
                n.lengthBeats = juce::jmax(0.15, (double)(stepBeats * lenMul));
                n.velocity = juce::jlimit(1, 127, (int)juce::jmap((float)r.nextDouble(), 80.0f, 112.0f));
                out.push_back(n);
            }
            beat += stepBeats;
        }
    }

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
        n.lengthBeats = juce::jmax(0.05, lenBeats);
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
        // Major-ish workhorses
        {{0,4,5,3}, 1.00f}, // I–V–vi–IV
        {{0,5,3,4}, 0.95f}, // I–vi–IV–V
        {{1,4,0,0}, 0.85f}, // ii–V–I–I
        {{0,3,4,3}, 0.80f}, // I–IV–V–IV
        {{5,3,0,4}, 0.75f}, // vi–IV–I–V
        {{0,1,4,0}, 0.70f}, // I–ii–V–I
        {{0,3,5,4}, 0.65f}, // I–IV–vi–V
        {{0,4,0,5}, 0.55f}, // I–V–I–vi
        {{0,3,0,4}, 0.55f}, // I–IV–I–V
        {{0,0,4,5}, 0.50f}, // I–I–V–vi (explicit repeat)
        {{4,3,0,4}, 0.45f}, // V–IV–I–V (turnaround feel)

        // Minor-leaning (still diatonic in chosen scale)
        {{0,5,3,4}, 0.70f}, // i–VI–IV–V
        {{0,3,4,3}, 0.65f}, // i–iv–V–iv
        {{0,4,5,4}, 0.60f}, // i–V–VI–V
        {{0,2,3,4}, 0.55f}, // i–III–iv–V
        {{0,5,0,4}, 0.50f}, // i–VI–i–V
        {{0,0,5,3}, 0.45f}, // i–i–VI–IV

        // Modal colours / motion
        {{0,2,4,5}, 0.55f}, // I–III–V–VI (modal lift)
        {{0,3,2,4}, 0.50f}, // I–IV–III–V
        {{0,6,5,4}, 0.45f}, // I–vii°–vi–V (or modal leading tone)
        {{0,2,1,4}, 0.45f}, // I–III–ii–V
        {{0,3,1,4}, 0.40f}, // I–IV–ii–V

        // Two-chord loops (great for vibe, will be expanded by mutators)
        {{0,4},    0.60f},  // I–V
        {{0,3},    0.60f},  // I–IV
        {{5,3},    0.55f},  // vi–IV
        {{0,5},    0.55f},  // I–vi
        {{1,4},    0.50f},  // ii–V
        {{3,4},    0.45f},  // IV–V

        // Ascend/descend ladders (diatonic stepwise functions)
        {{0,1,2,3,4}, 0.40f},
        {{0,6,5,4,3}, 0.35f},
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

// ===== Global rhythm segmenter (favor 1/4, 1/2, whole; throttle 1/8 & 1/16) =====
    std::vector<double> segPalette { 2.0, 1.0, 0.5, 0.5, 0.25 };  // beats
    if (feelAmt > 0.55f) segPalette.push_back(0.25);
    if (feelAmt < 0.25f)
        segPalette.erase(std::remove(segPalette.begin(), segPalette.end(), 0.25), segPalette.end());
    // If the meter is “super busy,” forbid short chord changes entirely.
    if (superBusyTS)
    {
        // Drop all entries <= 0.5 (i.e., eighths and sixteenths)
        segPalette.erase(
            std::remove_if(segPalette.begin(), segPalette.end(),
                [](double v) { return v <= 0.5 + 1e-9; }),
            segPalette.end()
        );

        // Safety: if somehow we removed everything (shouldn’t happen), fall back to quarters.
        if (segPalette.empty())
            segPalette = { 1.0, 2.0 };
    }
    // Heavily down-weight the "short" changes so they are unlikely even before the hard cap.
    // (We’ll also enforce a 10% hard cap below.)
    auto segWeight = [](double v)->float
    {
        if (v >= 3.99)                    return 0.80f; // whole (4.0) if you ever add it
        if (v >= 1.99 && v <= 2.01)       return 0.70f; // half (2.0)
        if (v >= 0.99 && v <= 1.01)       return 1.00f; // quarter (1.0) — most common
        if (v >= 0.49 && v <= 0.51)       return 0.08f; // eighth (0.5) — very rare
        /* else (≈0.25) */                 return 0.02f; // sixteenth (0.25) — extremely rare
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
        const double len = juce::jmax(0.08, cs.dur * (1.0 + lJit));

        const auto pcs = triadPCs(cs.degree);

        const int baseVel = juce::jlimit(50, 120,
            (int)juce::jmap((float)rand01(), 92.0f, 112.0f)
            + (int)juce::roundToInt(velHuman * 6.0f));

        const int p0 = toMidi(pcs[0], baseOct);
        const int p1 = toMidi(pcs[1], baseOct);
        const int p2 = toMidi(pcs[2], baseOct);

        // build the chord triad for this hit
        std::vector<Note> triad;
        {
            Note n0; n0.pitch = juce::jlimit(0, 127, p0); n0.velocity = baseVel;     n0.startBeats = start; n0.lengthBeats = len; n0.isOrnament = false; triad.push_back(n0);
            Note n1; n1.pitch = juce::jlimit(0, 127, p1); n1.velocity = baseVel - 5; n1.startBeats = start; n1.lengthBeats = len; n1.isOrnament = false; triad.push_back(n1);
            Note n2; n2.pitch = juce::jlimit(0, 127, p2); n2.velocity = baseVel - 8; n2.startBeats = start; n2.lengthBeats = len; n2.isOrnament = false; triad.push_back(n2);
        }

        // root midi for this chord (used by extensions)
        const int chordRootMidi = p0;

        // apply extensions / sus / alt / slash according to APVTS (via adv opts in generator)
        applyExtensionsAndOthers(triad, chordRootMidi);

        // stash for progression-wide advanced families
        progression.push_back(std::move(triad));
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

    return out;

}


std::vector<Note> MidiGenerator::generateChordTrack()
{
    // Reuse the chord generator you already implemented (it reads the same getters)
    return generateChords();
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
