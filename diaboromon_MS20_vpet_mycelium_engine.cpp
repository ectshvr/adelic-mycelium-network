#define MA_NO_ENGINE
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <iostream>
#include <fstream>
#include <complex>
#include <cmath>
#include <atomic>
#include <thread>
#include <chrono>
#include <vector>
#include <string>
#include <algorithm>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

// ============================================================================
// DIGIMON EVOLUTION STAGES
// ============================================================================
enum class DigiStage {
    KURAMON,
    TSUMEMON,
    KERAMON,
    CHRYSALIMON,
    INFERMON,
    DIABOROMON
};

// ============================================================================
// ATOMIC GLOBAL VARIABLES
// ============================================================================
std::atomic<float>    g_mic_energy{0.0f};
std::atomic<float>    g_synth_env{0.0f};      
std::atomic<uint64_t> g_brain_state{0};        
std::atomic<int>      g_stimulus_count{0};    
std::atomic<float>    g_tempo_hz{4.0f};        
std::atomic<int>      g_octave_shift{0};      
std::atomic<int>      g_preset_idx{0};         
std::atomic<bool>     g_running{true};

// ============================================================================
// SCALES & PRESET DEFINITIONS
// ============================================================================
const float SCALE_CYBER_MOLL[16] = {
    65.41f, 77.78f, 87.31f, 92.50f, 103.83f, 116.54f, 130.81f, 155.56f,
    174.61f, 185.00f, 207.65f, 233.08f, 261.63f, 311.13f, 349.23f, 392.00f
};

const float SCALE_INDUSTRIAL[16] = {
    32.70f, 48.00f, 65.41f, 73.20f, 82.41f, 104.00f, 110.00f, 135.00f,
    146.83f, 170.00f, 196.00f, 220.00f, 246.94f, 290.00f, 330.00f, 400.00f
};

const float SCALE_ALIEN_CHAOS[16] = {
    44.0f,  88.5f, 130.0f, 175.2f, 210.0f, 310.5f, 420.0f, 512.0f,
    600.0f, 750.0f, 890.0f, 1024.0f, 1200.0f, 1500.0f, 1800.0f, 2200.0f
};

struct PresetConfig {
    std::string name;
    int scale_type;        
    float default_tempo;
    float lpf_cutoff;      
    float lpf_res;         
    float hpf_cutoff;      
    float hpf_res;         
};

const PresetConfig PRESETS[6] = {
    {"01: Net Slime (Acid Wobble)",     0, 4.0f, 180.0f, 0.75f,  60.0f, 0.3f},
    {"02: Digimoji Glitch (Bit-Chaos)", 1, 6.0f, 320.0f, 0.92f, 150.0f, 0.6f},
    {"03: Overload Core (Screamer)",    1, 5.0f, 450.0f, 0.96f, 220.0f, 0.8f},
    {"04: Abyss Protocol (Sub-Drone)",  0, 2.5f, 250.0f, 0.50f, 800.0f, 0.9f}, 
    {"05: Zero-Day Worm (Stutter)",     2, 8.5f, 600.0f, 0.88f, 300.0f, 0.5f}, 
    {"06: Dark Area Apocalypse (Mega)", 2, 6.0f, 800.0f, 0.98f, 100.0f, 0.7f}  
};

// ============================================================================
// 17-CORE IRRATIONAL MYCELIUM NETWORK
// ============================================================================
class MyceliumBrain {
private:
    std::vector<uint64_t> nodes;
    const std::string save_filename = "mycelium_dna.dat";

    // 17 unique irrational constants as integer scalings for the cores
    const uint64_t IRRATIONAL_MULTIPLIERS[17] = {
        16180339887ULL, // 0: Golden Ratio (phi)
        31415926535ULL, // 1: Circle constant (pi)
        27182818284ULL, // 2: Euler's number (e)
        14142135623ULL, // 3: Square root of 2
        17320508075ULL, // 4: Square root of 3
        22360679774ULL, // 5: Square root of 5
        46692016091ULL, // 6: Feigenbaum constant (Delta)
        25029078750ULL, // 7: Feigenbaum Alpha
        26854520010ULL, // 8: Khinchin constant
        57721566490ULL, // 9: Euler-Mascheroni constant
        12020569031ULL, // 10: Apéry's constant zeta(3)
        56714329040ULL, // 11: Omega constant
        69314718056ULL, // 12: Natural logarithm of 2
        26457513110ULL, // 13: Square root of 7
        13247179572ULL, // 14: Plastic constant
        12824271291ULL, // 15: Glaisher-Kinkelin constant
        26149721284ULL  // 16: Meissel-Mertens constant
    };

public:
    MyceliumBrain() : nodes(17) {
        for (int i = 0; i < 17; ++i) {
            nodes[i] = IRRATIONAL_MULTIPLIERS[i] + (static_cast<uint64_t>(i) * 37ULL);
        }
        load_state();
    }

    inline void process_stimulus(uint8_t pitch_stim, float energy) {
        uint8_t energy_stim = static_cast<uint8_t>(std::clamp(energy * 200.0f, 1.0f, 16.0f));
        
        // Stimulus impacts the root node (0), weighted with phi
        nodes[0] = (nodes[0] * IRRATIONAL_MULTIPLIERS[0]) + (pitch_stim % 17) + energy_stim;

        // Propagation through the 17-core mycelium with individual irrational weights
        std::vector<uint64_t> next_nodes = nodes;
        for (int i = 0; i < 17; ++i) {
            int prev = (i + 16) % 17;
            int next = (i + 1) % 17;
            
            uint64_t mix = nodes[i] ^ (nodes[prev] >> 3) ^ (nodes[next] << 5);
            next_nodes[i] = (mix * IRRATIONAL_MULTIPLIERS[i]) + (pitch_stim % 17);
        }
        nodes = next_nodes;
    }

    uint64_t get_combined_state() const {
        uint64_t acc = 0;
        for (auto n : nodes) {
            acc ^= n;
        }
        return acc;
    }

    void save_state() const {
        std::ofstream outfile(save_filename, std::ios::binary);
        if (outfile.is_open()) {
            for (uint64_t n : nodes) {
                outfile.write(reinterpret_cast<const char*>(&n), sizeof(n));
            }
            std::cout << "\n>> Irrational Mycelium DNA saved (" << save_filename << ").\n";
        }
    }

    void load_state() {
        std::ifstream infile(save_filename, std::ios::binary);
        if (infile.is_open()) {
            for (int i = 0; i < 17; ++i) {
                infile.read(reinterpret_cast<char*>(&nodes[i]), sizeof(nodes[i]));
            }
        }
    }
};

// ============================================================================
// AMDF PITCH ANALYSIS (WITHOUT FFT)
// ============================================================================
float estimate_pitch_amdf(const std::vector<float>& history, float sampleRate) {
    int history_size = history.size();
    int min_lag = 40;  
    int max_lag = 700; 
    if (max_lag >= history_size / 2) max_lag = history_size / 2;

    float min_val = 1e9f;
    int best_lag = min_lag;

    for (int lag = min_lag; lag <= max_lag; lag += 3) {
        float sum = 0.0f;
        int limit = history_size - lag;
        int eval_len = std::min(limit, 512);
        for (int j = 0; j < eval_len; ++j) {
            sum += std::abs(history[history_size - 1 - j] - history[history_size - 1 - j - lag]);
        }
        float avg = sum / static_cast<float>(eval_len);
        if (avg < min_val) {
            min_val = avg;
            best_lag = lag;
        }
    }

    if (best_lag <= 0) return 150.0f;
    return sampleRate / static_cast<float>(best_lag);
}

// ============================================================================
// SYNTHESIZER STATE
// ============================================================================
struct SynthState {
    MyceliumBrain brain;
    double sampleRate = 44100.0;
    double phase1 = 0.0;
    double phase2 = 0.0;
    float env = 0.0f;
    
    int gate_hold_samples = 0; 
    
    float lpf_stage[4] = {0.0f};
    float lpf_delay[4] = {0.0f};
    float hpf_stage[2] = {0.0f};
    float hpf_delay[2] = {0.0f};
    
    double lfo_phase1 = 0.0;
    double lfo_phase2 = 0.0;
    double arp_timer = 0.0;
    int current_note_idx = 0;

    std::vector<float> mic_history{2048, 0.0f};
    size_t mic_history_idx = 0;
};

// MS-20 Filter Chain
float process_ms20_filters(float input, float lpf_cut, float lpf_r, float hpf_cut, float hpf_r, float sampleRate, SynthState* state) {
    float f_h = std::clamp(hpf_cut, 10.0f, 6000.0f) / sampleRate;
    float hp_coeff = 2.0f * std::sin(M_PI * f_h);
    hp_coeff = std::clamp(hp_coeff, 0.001f, 0.95f);
    
    float hpf_res = std::clamp(hpf_r, 0.0f, 0.98f) * 3.8f;
    float hp_input = std::tanh(input * 1.8f) - hpf_res * state->hpf_delay[1];
    
    state->hpf_stage[0] += hp_coeff * (hp_input - state->hpf_stage[0]);
    state->hpf_stage[1] += hp_coeff * (state->hpf_stage[0] - state->hpf_stage[1]);
    state->hpf_delay[1] = state->hpf_stage[1];
    
    float hpf_out = hp_input - state->hpf_stage[1];
    float driven_input = std::tanh(hpf_out * 2.5f);

    float cutoff = std::clamp(lpf_cut, 40.0f, 15000.0f);
    float res = std::clamp(lpf_r, 0.0f, 0.98f);

    float f_l = (2.0f * cutoff) / sampleRate;
    float k = 3.6f * f_l - 1.6f * f_l * f_l - 1.0f;
    float p = (k + 1.0f) * 0.5f;
    float scale = std::pow(1.0f - p, 1.3f) * 1.3f;
    float r = res * 4.2f * scale;

    float lp_x = driven_input - r * state->lpf_delay[3];

    state->lpf_stage[0] = lp_x * p + state->lpf_delay[0] * (1.0f - p);
    state->lpf_stage[1] = state->lpf_stage[0] * p + state->lpf_delay[1] * (1.0f - p);
    state->lpf_stage[2] = state->lpf_stage[1] * p + state->lpf_delay[2] * (1.0f - p);
    state->lpf_stage[3] = state->lpf_stage[2] * p + state->lpf_delay[3] * (1.0f - p);

    for (int i = 0; i < 4; ++i) state->lpf_delay[i] = state->lpf_stage[i];

    return std::tanh(state->lpf_stage[3] * 1.5f);
}

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    SynthState* state = static_cast<SynthState*>(pDevice->pUserData);
    float* out = static_cast<float*>(pOutput);
    const float* mic_in = static_cast<const float*>(pInput);

    float current_mic_energy = 0.0f;
    if (mic_in) {
        float sum_sq = 0.0f;
        for (ma_uint32 i = 0; i < frameCount; ++i) {
            sum_sq += mic_in[i] * mic_in[i];
            state->mic_history[state->mic_history_idx] = mic_in[i];
            state->mic_history_idx = (state->mic_history_idx + 1) % state->mic_history.size();
        }
        current_mic_energy = std::sqrt(sum_sq / static_cast<float>(frameCount));
        g_mic_energy.store(current_mic_energy);
    }

    int p_idx = g_preset_idx.load();
    const PresetConfig& preset = PRESETS[p_idx];

    const float SILENCE_THRESHOLD = 0.010f; 
    
    if (current_mic_energy >= SILENCE_THRESHOLD) {
        state->gate_hold_samples = 15400; 
    } else if (state->gate_hold_samples > 0) {
        state->gate_hold_samples -= static_cast<int>(frameCount);
    }

    bool is_active = (state->gate_hold_samples > 0);
    int stimuli = g_stimulus_count.load();

    for (ma_uint32 i = 0; i < frameCount; ++i) {
        if (!is_active) {
            state->env *= 0.993f; 
            if (state->env < 0.001f) state->env = 0.0f;
        } else {
            double current_tempo = static_cast<double>(g_tempo_hz.load());
            state->arp_timer += current_tempo / state->sampleRate;
            
            if (state->arp_timer >= 1.0) {
                state->arp_timer -= 1.0;
                
                float estimated_freq = estimate_pitch_amdf(state->mic_history, static_cast<float>(state->sampleRate));
                uint8_t pitch_stim = static_cast<uint8_t>(std::clamp((estimated_freq - 80.0f) / 48.0f, 0.0f, 15.0f));

                state->brain.process_stimulus(pitch_stim, current_mic_energy);
                g_stimulus_count.fetch_add(1);
                
                uint64_t combined_state = state->brain.get_combined_state();
                g_brain_state.store(combined_state);
                
                state->current_note_idx = (combined_state & 0x0F);
                state->env = 1.0f; 
            }
            
            state->env *= (stimuli > 150) ? 0.9996f : 0.9990f;
        }

        uint64_t b_state = g_brain_state.load();
        uint8_t filter_cv = ((b_state >> 4) & 0x7F);      
        uint8_t lfo_cv    = ((b_state >> 11) & 0x1F);     

        float raw_freq = SCALE_CYBER_MOLL[state->current_note_idx];
        if (preset.scale_type == 1) raw_freq = SCALE_INDUSTRIAL[state->current_note_idx];
        if (preset.scale_type == 2) raw_freq = SCALE_ALIEN_CHAOS[state->current_note_idx];

        int oct_shift = g_octave_shift.load();
        float target_freq = raw_freq * std::pow(2.0f, static_cast<float>(oct_shift));
        
        double phase_inc1 = (2.0 * M_PI * target_freq) / state->sampleRate;
        double phase_inc2 = (2.0 * M_PI * target_freq * 1.015) / state->sampleRate; 
        
        state->phase1 += phase_inc1; if (state->phase1 >= 2.0 * M_PI) state->phase1 -= 2.0 * M_PI;
        state->phase2 += phase_inc2; if (state->phase2 >= 2.0 * M_PI) state->phase2 -= 2.0 * M_PI;

        float vco1 = static_cast<float>(1.0 - (state->phase1 / M_PI)); 
        float vco2 = (std::sin(state->phase2) > 0.0) ? 1.0f : -1.0f;   

        float raw_osc = vco1;
        if (stimuli > 40) raw_osc = (vco1 * 0.4f) + (vco2 * 0.6f);
        if (stimuli > 120) {
            float ring = vco1 * vco2;
            float sub = (std::sin(state->phase1 * 0.5) > 0.0) ? 0.5f : -0.5f;
            raw_osc = (ring * 0.7f) + sub;
        }

        state->lfo_phase1 += (2.0 * M_PI * (0.8f + (lfo_cv * 0.12f))) / state->sampleRate;
        if (state->lfo_phase1 >= 2.0 * M_PI) state->lfo_phase1 -= 2.0 * M_PI;
        float lfo1 = static_cast<float>(std::sin(state->lfo_phase1));

        state->lfo_phase2 += (2.0 * M_PI * 0.25f) / state->sampleRate;
        if (state->lfo_phase2 >= 2.0 * M_PI) state->lfo_phase2 -= 2.0 * M_PI;
        float lfo2 = static_cast<float>(std::cos(state->lfo_phase2));

        float lpf_cutoff = preset.lpf_cutoff + (filter_cv * 40.0f) + (state->env * 3000.0f) + (lfo1 * 400.0f);
        float hpf_cutoff = preset.hpf_cutoff + (lfo2 * 200.0f);

        float filtered = process_ms20_filters(raw_osc, lpf_cutoff, preset.lpf_res, hpf_cutoff, preset.hpf_res, static_cast<float>(state->sampleRate), state);
        float synth_sample = filtered * state->env * 0.45f;

        float vocal_sample = (mic_in != nullptr) ? mic_in[i] * 3.5f : 0.0f;
        float final_sample = std::tanh(synth_sample + vocal_sample);

        out[i * 2 + 0] = final_sample;
        out[i * 2 + 1] = final_sample;
    }
    
    g_synth_env.store(state->env);
}

// ============================================================================
// SPRITES & UI (MOUTH ANIMATION)
// ============================================================================
std::vector<std::string> get_sprite(DigiStage stage, bool mouth_open) {
    std::vector<std::string> grid(8, "                         ");

    if (stage == DigiStage::KURAMON) {
        grid[2] = "                    ▄████▄                   ";
        grid[3] = "                    ██ ▀██▀ ██                   ";
        grid[4] = "                    ██ ▄██▄ ██                   ";
        grid[5] = mouth_open ? "                    \\ (O) /                   " : "                    ▀████▀                   ";
    } 
    else if (stage == DigiStage::TSUMEMON) {
        grid[2] = "                  ▲ ▄███▄ ▲                   ";
        grid[3] = "                  ▄██████████▄                  ";
        grid[4] = "                  ██ █▀  ▀█ ██                  ";
        grid[5] = mouth_open ? "                  \\ (O) /                    " : "                  ▀███████▀                   ";
    }
    else if (stage == DigiStage::KERAMON) {
        grid[1] = "                  █▄  ▄███▄  ▄█                 ";
        grid[2] = "                   ███████████                  ";
        grid[3] = "                   ██ █▀  ▀█ ██                 ";
        grid[4] = "                   ██ ▀███▀  ██                 ";
        grid[5] = mouth_open ? "                    \\ (O) /                  " : "                   ▀███████▀                  ";
    }
    else if (stage == DigiStage::CHRYSALIMON) {
        grid[1] = "                    ▄████▄                   ";
        grid[2] = "                  ██▀██▀██                  ";
        grid[3] = "                  ██ █  █ ██                 ";
        grid[4] = "                  ██▄██▄██                  ";
        grid[5] = mouth_open ? "                  / █  █ \\                  " : "                  ▀ █  █ ▀                  ";
    }
    else if (stage == DigiStage::INFERMON) {
        grid[2] = "                     ▄███▄                   ";
        grid[3] = "                    ███████                  ";
        grid[4] = "                     ██ █ █ ██               ";
        grid[5] = mouth_open ? "                     \\ (O) /                  " : "                     ▀█████▀                  ";
    }
    else if (stage == DigiStage::DIABOROMON) {
        grid[1] = "                     ▲  ██  ▲                 ";
        grid[2] = "                  ▄██████████▄                ";
        grid[3] = "                 ██ █▀ █ ▀█ ██                ";
        grid[4] = "                 ██  ██████  ██                ";
        grid[5] = mouth_open ? "                 / ▄█    █▄ \\                " : "                 ▀ ▄█    █▄ ▀                ";
    }
    return grid;
}

void setup_terminal(termios& orig_opts) {
    tcgetattr(STDIN_FILENO, &orig_opts);
    termios raw_opts = orig_opts;
    raw_opts.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &raw_opts);
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    std::cout << "\033[?25l" << std::flush;
}

void restore_terminal(termios& orig_opts) {
    std::cout << "\033[?25h" << std::flush;
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_opts);
}

void draw_ascii_art() {
    float env = g_synth_env.load();
    int p_idx = g_preset_idx.load();
    int stimuli = g_stimulus_count.load();
    bool mouth_open = (env > 0.15f);

    DigiStage stage = DigiStage::KURAMON;
    std::string stage_name = "KURAMON (Baby)";
    if (stimuli > 30)  { stage = DigiStage::TSUMEMON;    stage_name = "TSUMEMON (In-Training)"; }
    if (stimuli > 70)  { stage = DigiStage::KERAMON;     stage_name = "KERAMON (Rookie)"; }
    if (stimuli > 120) { stage = DigiStage::CHRYSALIMON; stage_name = "CHRYSALIMON (Champion)"; }
    if (stimuli > 180) { stage = DigiStage::INFERMON;    stage_name = "INFERMON (Ultimate)"; }
    if (stimuli > 250) { stage = DigiStage::DIABOROMON;  stage_name = "DIABOROMON (Mega - Irrational Mycelium)"; }

    auto sprite = get_sprite(stage, mouth_open);

    std::cout << "\033[H"; 
    std::cout << "=== IRRATIONAL MYCELIUM 17-CORE WORKSTATION ===\n";
    std::cout << "Preset: " << PRESETS[p_idx].name << "\n";
    std::cout << "Tempo: " << g_tempo_hz.load() << " Hz | Oct: " << g_octave_shift.load() << " | Stimuli: " << stimuli << "\n";
    std::cout << "---------------------------------------------\n";
    std::cout << "Evo: " << stage_name << "\n";

    for (const auto& line : sprite) {
        std::cout << line << "\n";
    }

    std::cout << "---------------------------------------------\n";
    std::cout << "[P] Preset | [+,-] Tempo | [U,D] Octave | [SPACE] Trigger | [E] Evo | [ESC] Save & Quit\n";
    std::cout << std::flush;
}

// ============================================================================
// MAIN
// ============================================================================
int main() {
    std::cout << "\033[?1049h\033[H";

    SynthState synth_state;

    ma_device_config config = ma_device_config_init(ma_device_type_duplex);
    config.playback.format   = ma_format_f32;
    config.playback.channels = 2;
    config.capture.format    = ma_format_f32;
    config.capture.channels  = 1;
    config.sampleRate        = static_cast<ma_uint32>(synth_state.sampleRate);
    config.dataCallback      = data_callback;
    config.pUserData         = &synth_state;
    config.periodSizeInFrames = 256;

    // Use default system audio devices (automatically routes to standard mic & speakers)
    config.playback.pDeviceID = NULL;
    config.capture.pDeviceID  = NULL;

    ma_device device;
    if (ma_device_init(NULL, &config, &device) != MA_SUCCESS) {
        std::cout << "\033[?1049l";
        std::cerr << ">> Error: Failed to initialize audio device.\n";
        return -1;
    }
    
    if (ma_device_start(&device) != MA_SUCCESS) {
        ma_device_uninit(&device); 
        std::cout << "\033[?1049l";
        std::cerr << ">> Error: Failed to start audio device.\n";
        return -1;
    }

    termios orig_opts;
    setup_terminal(orig_opts);

    while (g_running.load()) {
        draw_ascii_art();

        char c;
        while (read(STDIN_FILENO, &c, 1) > 0) {
            float current_tempo = g_tempo_hz.load();
            int current_oct = g_octave_shift.load();
            int current_p = g_preset_idx.load();
            int cur_s = g_stimulus_count.load();

            switch (c) {
                case 'p': case 'P':
                    current_p = (current_p + 1) % 6; 
                    g_preset_idx.store(current_p);
                    g_tempo_hz.store(PRESETS[current_p].default_tempo);
                    break;
                case '+': case '=': 
                    current_tempo = std::min(15.0f, current_tempo + 0.5f);
                    g_tempo_hz.store(current_tempo);
                    break;
                case '-': case '_': 
                    current_tempo = std::max(1.0f, current_tempo - 0.5f);
                    g_tempo_hz.store(current_tempo);
                    break;
                case 'u': case 'U':
                    if (current_oct < 2) g_octave_shift.store(current_oct + 1);
                    break;
                case 'd': case 'D':
                    if (current_oct > -2) g_octave_shift.store(current_oct - 1);
                    break;
                case 'e': case 'E':
                    if (cur_s <= 30) g_stimulus_count.store(31);
                    else if (cur_s <= 70) g_stimulus_count.store(71);
                    else if (cur_s <= 120) g_stimulus_count.store(121);
                    else if (cur_s <= 180) g_stimulus_count.store(181);
                    else g_stimulus_count.store(251);
                    break;
                case ' ': 
                    synth_state.brain.process_stimulus(8, 0.5f);
                    synth_state.env = 1.0f;
                    synth_state.gate_hold_samples = 15400;
                    g_stimulus_count.fetch_add(1);
                    break;
                case 27: // ESC -> Save & Exit
                    synth_state.brain.save_state();
                    g_running.store(false);
                    break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    restore_terminal(orig_opts);
    ma_device_uninit(&device);
    
    std::cout << "\033[?1049l";
    std::cout << ">> System offline. Irrational Mycelium DNA secured.\n";

    return 0;
}
