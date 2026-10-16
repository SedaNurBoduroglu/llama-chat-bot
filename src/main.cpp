#include "llama.h"
#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>
#include <fstream>
#include <sys/ioctl.h>
#include <unistd.h>

// --- Retro Arcade UI Constants (ANSI Escape Codes) ---
#define C_CYAN        "\033[38;2;0;255;255m"
#define C_MAGENTA     "\033[38;2;255;0;255m"
#define C_GREEN       "\033[1;32m"
#define C_YELLOW      "\033[1;33m"
#define C_NEON_ORANGE "\033[38;2;255;102;0m"
#define C_RED         "\033[1;31m"
#define C_DIM         "\033[2m"
#define C_RESET       "\033[0m"
#define C_CLEAR       "\033[2J\033[H"

// ---------- Terminal yardımcıları ----------

static int term_cols() {
    struct winsize w{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0) return w.ws_col;
    return 80; // fallback
}

// Cursor konumunu kaydet -> istenen satıra git -> yaz -> eski konuma dön
static void draw_at_row(int row, const std::string& left, const std::string& right, const char* left_color) {
    int cols = term_cols();
    std::cout << "\0337";                     // cursor kaydet (DECSC)
    std::cout << "\033[" << row << ";1H";      // satıra git
    std::cout << "\033[K";                     // satırı temizle

    int pad = cols - (int)left.size() - (int)right.size();
    if (pad < 1) pad = 1;

    std::cout << left_color << left << C_RESET
               << std::string(pad, ' ')
               << C_NEON_ORANGE << right << C_RESET;

    std::cout << "\0338" << std::flush;        // cursor geri yükle (DECRC)
}

// /proc üzerinden RSS (kullanılan RAM, MB) ve toplam RAM (MB)
static void get_mem_mb(double& used_mb, double& total_mb) {
    used_mb = 0; total_mb = 0;
    std::ifstream status("/proc/self/status");
    std::string line;
    while (std::getline(status, line)) {
        if (line.rfind("VmRSS:", 0) == 0) {
            std::istringstream iss(line.substr(6));
            long kb; iss >> kb;
            used_mb = kb / 1024.0;
            break;
        }
    }
    std::ifstream meminfo("/proc/meminfo");
    while (std::getline(meminfo, line)) {
        if (line.rfind("MemTotal:", 0) == 0) {
            std::istringstream iss(line.substr(9));
            long kb; iss >> kb;
            total_mb = kb / 1024.0;
            break;
        }
    }
}

static std::string fmt(double v, int prec) {
    std::ostringstream o;
    o << std::fixed << std::setprecision(prec) << v;
    return o.str();
}

struct Stats {
    int n_ctx_used = 0, n_ctx_total = 0;
    double pp_tok_s = 0.0;   // prompt processing hızı
    double gen_tok_s = 0.0;  // üretim hızı
    double ftl_s = 0.0;      // first-token latency
};

static void print_header(const std::string& model_name, const Stats& s) {
    double used_mb, total_mb;
    get_mem_mb(used_mb, total_mb);

    std::ostringstream left;
    left << " GEMSTONE OS  //  " << model_name;

    std::ostringstream right;
    right << fmt(used_mb, 0) << "/" << fmt(total_mb, 0) << " MB"
          << "  ctx " << s.n_ctx_used << "/" << s.n_ctx_total
          << "  pp " << fmt(s.pp_tok_s, 1)
          << "  ftl " << fmt(s.ftl_s, 2) << "s"
          << "  " << fmt(s.gen_tok_s, 1) << " tok/s ";

    draw_at_row(1, left.str(), right.str(), C_CYAN);

    int cols = term_cols();
    std::cout << "\0337\033[2;1H\033[K" << C_MAGENTA
               << std::string(cols, '=') << C_RESET << "\0338" << std::flush;
}

static void print_status_bar(const std::string& text, bool busy) {
    int row = 999; // terminal alt satırına gitmek için büyük değer -> terminal kendi sınırında durur
    std::cout << "\0337\033[" << row << ";1H\033[2K";
    std::cout << (busy ? C_YELLOW : C_DIM) << ">> " << text << C_RESET;
    std::cout << "\0338" << std::flush;
}

static void print_arcade_header_static() {
    std::cout << C_CLEAR;
    // Üst iki satırı draw_at_row / print_header dinamik olarak yönetecek,
    // burada sadece içerik alanına biraz boşluk bırakıyoruz.
    std::cout << "\n\n";
}

// --- Boş Log Fonksiyonu (Logları Gizlemek İçin) ---
static void llama_log_callback(ggml_log_level level, const char* text, void* user_data) {
    (void)level; (void)text; (void)user_data;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << C_RED << "[SYS_ERROR] Missing model. Usage: ./gemstone_os_arm <path_to_gguf> [n_gpu_layers]\n" << C_RESET;
        return 1;
    }
    std::string model_path = argv[1];
    int n_gpu_layers = (argc >= 3) ? std::atoi(argv[2]) : 0; // isteğe bağlı GPU offload

    // Model adını dosya yolundan çıkar (sade gösterim için)
    std::string model_name = model_path.substr(model_path.find_last_of("/\\") + 1);

    print_arcade_header_static();
    llama_backend_init();
    llama_log_set(llama_log_callback, nullptr);

    llama_model_params model_params = llama_model_default_params();
    model_params.n_gpu_layers = n_gpu_layers;

    llama_model* model = llama_model_load_from_file(model_path.c_str(), model_params);
    if (!model) {
        std::cerr << C_RED << "[SYS_ERROR] Model could not be loaded.\n" << C_RESET;
        return 1;
    }

    const llama_vocab* vocab = llama_model_get_vocab(model);

    llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx   = 1024;
    ctx_params.n_batch = 256;

    // Çekirdek sayısını otomatik al (ör. A53 quad-core -> 4).
    unsigned hw = std::thread::hardware_concurrency();
    int n_threads = hw > 0 ? (int)hw : 4;
    ctx_params.n_threads       = n_threads;
    ctx_params.n_threads_batch = n_threads;

    llama_context* ctx = llama_init_from_model(model, ctx_params);
    if (!ctx) {
        std::cerr << C_RED << "[SYS_ERROR] Context could not be created.\n" << C_RESET;
        llama_model_free(model);
        return 1;
    }

    auto sparams = llama_sampler_chain_default_params();
    llama_sampler* smpl = llama_sampler_chain_init(sparams);
    llama_sampler_chain_add(smpl, llama_sampler_init_temp(0.6f));
    llama_sampler_chain_add(smpl, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));

    int n_cur = 0;
    Stats stats;
    stats.n_ctx_total = ctx_params.n_ctx;

    print_header(model_name, stats);
    print_status_bar("idle - type a prompt", false);

    const std::string system_prompt =
        "You are a helpful assistant. Answer concisely and stop after your answer.";

    while (true) {
        std::cout << "\n" << C_CYAN << " USER: " << C_RESET;
        std::string user_input;
        if (!std::getline(std::cin, user_input) || user_input == "exit") {
            print_status_bar("shutting down...", true);
            std::cout << C_YELLOW << "\n[SHUTTING DOWN GEMSTONE CO-PILOT...]\n" << C_RESET;
            break;
        }
        if (user_input.empty()) continue;

        print_status_bar("generating - press ctrl+c to stop", true);

        // Chat template ile sar. İlk turda system + user, sonraki turlarda sadece user.
        // NOT: msgs dizisinin ömrü bu blokla sınırlı olduğundan, içindeki const char*
        // pointer'lar system_prompt ve user_input gibi bu bloğun sonuna kadar yaşayan
        // string'lere işaret etmeli (geçici string'lerin .c_str()'ine değil).
        llama_chat_message msgs[2];
        int n_msgs = 0;

        if (n_cur == 0) {
            msgs[0] = { "system", system_prompt.c_str() };
            msgs[1] = { "user", user_input.c_str() };
            n_msgs = 2;
        } else {
            msgs[0] = { "user", user_input.c_str() };
            n_msgs = 1;
        }

        std::vector<char> formatted(user_input.size() * 4 + system_prompt.size() + 256);
        const char* tmpl = llama_model_chat_template(model, nullptr);

        int32_t n_formatted = llama_chat_apply_template(
            tmpl, msgs, n_msgs, /*add_ass=*/true, formatted.data(), (int32_t)formatted.size());

        if (n_formatted < 0) {
            std::cerr << C_RED << "\n[SYS_ERROR] Chat template formatting failed.\n" << C_RESET;
            continue;
        }
        if (n_formatted > (int)formatted.size()) {
            formatted.resize(n_formatted);
            n_formatted = llama_chat_apply_template(
                tmpl, msgs, n_msgs, true, formatted.data(), (int32_t)formatted.size());
        }
        std::string prompt(formatted.data(), n_formatted);

        std::vector<llama_token> tokens(prompt.length() + 32);
        int n_tokens = llama_tokenize(vocab, prompt.c_str(), (int32_t)prompt.length(),
                                       tokens.data(), (int32_t)tokens.size(), (n_cur == 0), true);
        if (n_tokens < 0) {
            tokens.resize(-n_tokens);
            n_tokens = llama_tokenize(vocab, prompt.c_str(), (int32_t)prompt.length(),
                                       tokens.data(), (int32_t)tokens.size(), (n_cur == 0), true);
        }
        tokens.resize(n_tokens);

        // Context taşmasına karşı basit koruma
        if (n_cur + n_tokens >= stats.n_ctx_total) {
            std::cout << C_RED << "\n[SYS_ERROR] Context window full. Please restart.\n" << C_RESET;
            break;
        }

        auto pp_start = std::chrono::high_resolution_clock::now();
        llama_batch batch = llama_batch_get_one(tokens.data(), n_tokens);
        if (llama_decode(ctx, batch) != 0) {
            std::cerr << C_RED << "\n[SYS_ERROR] llama_decode failed on prompt.\n" << C_RESET;
            break;
        }
        n_cur += n_tokens;
        auto pp_end = std::chrono::high_resolution_clock::now();
        double pp_elapsed = std::chrono::duration<double>(pp_end - pp_start).count();
        stats.pp_tok_s = pp_elapsed > 0 ? n_tokens / pp_elapsed : 0.0;
        stats.n_ctx_used = n_cur;
        print_header(model_name, stats);

        std::cout << C_MAGENTA << " LLAMA: " << C_RESET << std::flush;

        const int max_new_tokens = 512;
        int generated_tokens = 0;
        std::string output_buffer;

        auto start_time = std::chrono::high_resolution_clock::now();
        bool first_token = true;
        bool ctx_overflow = false;

        for (int i = 0; i < max_new_tokens; ++i) {
            if (n_cur >= stats.n_ctx_total) { ctx_overflow = true; break; }

            llama_token new_token_id = llama_sampler_sample(smpl, ctx, -1);
            llama_sampler_accept(smpl, new_token_id);

            if (llama_vocab_is_eog(vocab, new_token_id)) break;

            char buf[128];
            int n = llama_token_to_piece(vocab, new_token_id, buf, sizeof(buf), 0, false);

            if (n >= 0) {
                std::string piece(buf, n);
                output_buffer += piece;

                if (first_token) {
                    stats.ftl_s = std::chrono::duration<double>(
                        std::chrono::high_resolution_clock::now() - start_time).count();
                    first_token = false;
                }

                if (output_buffer.find("User:") != std::string::npos ||
                    output_buffer.find("USER:") != std::string::npos ||
                    output_buffer.find("<|eot_id|>") != std::string::npos) break;

                std::cout << piece << std::flush;
            }

            generated_tokens++;

            llama_batch next_batch = llama_batch_get_one(&new_token_id, 1);
            if (llama_decode(ctx, next_batch) != 0) break;
            n_cur++;

            // Her ~8 tokende bir üst barı canlı güncelle (her tokende güncellemek maliyetli)
            if (generated_tokens % 8 == 0) {
                double elapsed_so_far = std::chrono::duration<double>(
                    std::chrono::high_resolution_clock::now() - start_time).count();
                stats.gen_tok_s = elapsed_so_far > 0 ? generated_tokens / elapsed_so_far : 0.0;
                stats.n_ctx_used = n_cur;
                print_header(model_name, stats);
            }
        }
        std::cout << C_RESET << "\n";

        // Asistan cevabını KV cache'te kapat. EOG token'ı yukarıdaki döngüde decode
        // edilmediği için, bu yapılmazsa bir sonraki tur şablonu kapanmamış bir
        // asistan mesajının arkasına eklenir ve model önceki cevabın devam ettiğini sanar.
        llama_token eot = llama_vocab_eot(vocab);
        if (eot == LLAMA_TOKEN_NULL) eot = llama_vocab_eos(vocab);
        if (eot != LLAMA_TOKEN_NULL && n_cur < stats.n_ctx_total) {
            llama_batch eot_batch = llama_batch_get_one(&eot, 1);
            if (llama_decode(ctx, eot_batch) == 0) n_cur++;
        }

        if (ctx_overflow) {
            std::cout << C_RED << "[SYS_WARN] Context window filled during generation.\n" << C_RESET;
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        double elapsed = std::chrono::duration<double>(end_time - start_time).count();
        stats.gen_tok_s = elapsed > 0 ? generated_tokens / elapsed : 0.0;
        stats.n_ctx_used = n_cur;
        print_header(model_name, stats);
        print_status_bar("idle - type a prompt", false);
    }

    llama_sampler_free(smpl);
    llama_free(ctx);
    llama_model_free(model);
    llama_backend_free();
    return 0;
}