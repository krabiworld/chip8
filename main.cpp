#include <fstream>
#include <iostream>
#include <random>
#include <thread>
#include <vector>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_events.h>

struct Chip8 {
    uint8_t memory[4096]{};
    uint8_t V[16]{};
    uint16_t I = 0;
    uint16_t PC = 0x200;
    uint8_t delayTimer = 0;
    uint8_t soundTimer = 0;
    uint16_t stack[16]{};
    uint8_t SP = 0;

    static constexpr int WIDTH = 64;
    static constexpr int HEIGHT = 32;
    uint8_t gfx[WIDTH * HEIGHT]{};
    bool drawFlag = false;

    uint8_t keys[16]{};

    std::mt19937 rng{std::random_device{}()};

    Chip8() {
        static const uint8_t fontset[80] = {
            0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
            0x20, 0x60, 0x20, 0x20, 0x70, // 1
            0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
            0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
            0x90, 0x90, 0xF0, 0x10, 0x10, // 4
            0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
            0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
            0xF0, 0x10, 0x20, 0x40, 0x40, // 7
            0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
            0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
            0xF0, 0x90, 0xF0, 0x90, 0x90, // A
            0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
            0xF0, 0x80, 0x80, 0x80, 0xF0, // C
            0xE0, 0x90, 0x90, 0x90, 0xE0, // D
            0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
            0xF0, 0x80, 0xF0, 0x80, 0x80  // F
        };

        for (int i = 0; i < 80; i++) {
            memory[i] = fontset[i];
        }
    }

    bool loadROM(const std::string &path) {
        std::ifstream file{path, std::ios::binary};
        if (!file) return false;

        std::vector<uint8_t> buffer((std::istreambuf_iterator(file)), std::istreambuf_iterator<char>());

        std::cout << "ROM size: " << buffer.size() << " bytes\n";

        if (buffer.size() + 0x200 > sizeof(memory)) return false;

        std::memcpy(memory + 0x200, buffer.data(), buffer.size());
        return true;
    }

    void step() {
        const uint16_t opcode = memory[PC] << 8 | memory[PC + 1];

        const uint8_t x = opcode >> 8 & 0x000F;
        const uint8_t y = opcode >> 4 & 0x000F;
        const uint8_t n = opcode & 0x000F;
        const uint8_t nn = opcode & 0x00FF;
        const uint16_t nnn = opcode & 0x0FFF;

        uint16_t nextPC = PC + 2;

        switch (opcode & 0xF000) {
            case 0x0000:
                switch (nn) {
                    case 0x00E0:
                        std::fill(std::begin(gfx), std::end(gfx), 0);
                        drawFlag = true;
                        break;
                    case 0x00EE:
                        nextPC = stack[--SP];
                        break;
                    default:
                        unknownOpcode(opcode);
                }
                break;
            case 0x1000:
                nextPC = nnn;
                break;
            case 0x2000:
                stack[SP++] = nextPC;
                nextPC = nnn;
                break;
            case 0x3000:
                nextPC = V[x] == nn ? PC + 4 : PC + 2;
                break;
            case 0x4000:
                nextPC = V[x] != nn ? PC + 4 : PC + 2;
                break;
            case 0x5000:
                nextPC = V[x] == V[y] ? PC + 4 : PC + 2;
                break;
            case 0x6000:
                V[x] = nn;
                break;
            case 0x7000:
                V[x] += nn;
                break;
            case 0x8000:
                switch (n) {
                    case 0x0:
                        V[x] = V[y];
                        break;
                    case 0x1:
                        V[x] = V[x] | V[y];
                        break;
                    case 0x2:
                        V[x] = V[x] & V[y];
                        break;
                    case 0x3:
                        V[x] = V[x] ^ V[y];
                        break;
                    case 0x4:
                        V[0xF] = V[x] + V[y] > 255 ? 1 : 0;
                        V[x] += V[y];
                        break;
                    case 0x5:
                        V[0xF] = V[x] > V[y] ? 1 : 0;
                        V[x] -= V[y];
                        break;
                    case 0x6:
                        V[0xF] = V[x] & 0x1;
                        V[x] = V[x] >> 1;
                        break;
                    case 0x7:
                        V[0xF] = V[y] > V[x] ? 1 : 0;
                        V[x] = V[y] - V[x];
                        break;
                    case 0xE:
                        V[0xF] = V[x] >> 7 & 0x1;
                        V[x] = V[x] << 1;
                        break;
                    default:
                        unknownOpcode(opcode);
                }
                break;
            case 0x9000:
                if (n) {
                    nextPC = V[x] != V[y] ? PC + 4 : PC + 2;
                } else {
                    unknownOpcode(opcode);
                }
                break;
            case 0xA000:
                I = nnn;
                break;
            case 0xB000:
                nextPC = V[0] + nnn;
                break;
            case 0xC000:
                V[x] = rng() % 256 & nn;
                break;
            case 0xD000:
                drawSprite(V[x], V[y], n);
                break;
            case 0xE000:
                switch (nn) {
                    case 0x9E:
                        nextPC = keys[V[x]] ? PC + 4 : PC + 2;
                        break;
                    case 0xA1:
                        nextPC = !keys[V[x]] ? PC + 4 : PC + 2;
                        break;
                    default:
                        unknownOpcode(opcode);
                }
                break;
            case 0xF000:
                switch (nn) {
                    case 0x07:
                        V[x] = delayTimer;
                        break;
                    case 0x0A: {
                        bool keyPressed = false;

                        for (int i = 0; i < 16; i++) {
                            if (keys[i]) {
                                V[x] = i;
                                keyPressed = true;
                                break;
                            }
                        }

                        if (!keyPressed) {
                            nextPC -= 2;
                        }
                        break;
                    }
                    case 0x15:
                        delayTimer = V[x];
                        break;
                    case 0x18:
                        soundTimer = V[x];
                        break;
                    case 0x1E:
                        V[0xF] = I + V[x] > 0xfff ? 1 : 0;
                        I = I + V[x];
                        break;
                    case 0x29:
                        I = 5 * V[x];
                        break;
                    case 0x33:
                        memory[I]     = V[x] / 100;
                        memory[I + 1] = V[x] / 10 % 10;
                        memory[I + 2] = V[x] % 10;
                        break;
                    case 0x55:
                        for (int i = 0; i <= x; i++) { memory[I + i] = V[i]; }
                        break;
                    case 0x65:
                        for (int i = 0; i <= x; i++) { V[i] = memory[I + i]; }
                        break;
                    default:
                        unknownOpcode(opcode);
                }
                break;
            default:
                unknownOpcode(opcode);
        }

        PC = nextPC;
    }

    void drawSprite(const uint8_t xPos, const uint8_t yPos, const uint8_t height) {
        V[0xF] = 0;

        for (int row = 0; row < height; row++) {
            const uint8_t spriteByte = memory[I + row];

            for (int col = 0; col < 8; col++) {
                if ((spriteByte & 0x80 >> col) != 0) {
                    const int x = (xPos + col) % WIDTH;
                    const int y = (yPos + row) % HEIGHT;
                    const int index = y * WIDTH + x;

                    if (gfx[index] == 1) {
                        V[0xF] = 1;
                    }
                    gfx[index] ^= 1;
                }
            }
        }

        drawFlag = true;
    }

    static int mapSDLKeyToChip8(const SDL_Scancode sc) {
        switch (sc) {
            case SDL_SCANCODE_1: return 0x1;
            case SDL_SCANCODE_2: return 0x2;
            case SDL_SCANCODE_3: return 0x3;
            case SDL_SCANCODE_4: return 0xC;

            case SDL_SCANCODE_Q: return 0x4;
            case SDL_SCANCODE_W: return 0x5;
            case SDL_SCANCODE_E: return 0x6;
            case SDL_SCANCODE_R: return 0xD;

            case SDL_SCANCODE_A: return 0x7;
            case SDL_SCANCODE_S: return 0x8;
            case SDL_SCANCODE_D: return 0x9;
            case SDL_SCANCODE_F: return 0xE;

            case SDL_SCANCODE_Z: return 0xA;
            case SDL_SCANCODE_X: return 0x0;
            case SDL_SCANCODE_C: return 0xB;
            case SDL_SCANCODE_V: return 0xF;

            default: return -1;
        }
    }

    static void unknownOpcode(const uint16_t opcode) {
        std::cout << "Unknown opcode: " << std::hex << opcode << "\n";
    }
};

int main(const int argc, char *argv[]) {
    if (argc < 2) {
        std::cout << "Usage: chip8 ./rom.ch8\n";
        return 1;
    }

    Chip8 chip8;
    if (!chip8.loadROM(argv[1])) {
        std::cerr << "Failed to load ROM\n";
        return 1;
    }

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS);
    SDL_Window *window = SDL_CreateWindow("CHIP-8 Emulator",
                                          Chip8::WIDTH * 10,
                                          Chip8::HEIGHT * 10,
                                          0);

    SDL_Renderer *renderer = SDL_CreateRenderer(window, nullptr);

    SDL_SetRenderVSync(renderer, 1);

    SDL_Texture *texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_STREAMING,
        Chip8::WIDTH,
        Chip8::HEIGHT
    );

    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);

    bool running = true;

    constexpr int CPU_HZ    = 400;
    constexpr int TIMER_HZ  = 60;
    constexpr int CPU_DELAY   = 1000 / CPU_HZ;
    constexpr int TIMER_DELAY = 1000 / TIMER_HZ;

    SDL_Event e;

    auto lastCpuTick   = std::chrono::high_resolution_clock::now();
    auto lastTimerTick = std::chrono::high_resolution_clock::now();

    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) running = false;

            if (e.type == SDL_EVENT_KEY_DOWN) {
                if (const int k = Chip8::mapSDLKeyToChip8(e.key.scancode); k != -1) chip8.keys[k] = 1;
            }
            if (e.type == SDL_EVENT_KEY_UP) {
                if (const int k = Chip8::mapSDLKeyToChip8(e.key.scancode); k != -1) chip8.keys[k] = 0;
            }
        }

        auto now = std::chrono::high_resolution_clock::now();

        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastCpuTick).count() >= CPU_DELAY) {
            chip8.step();
            lastCpuTick = now;
        }

        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTimerTick).count() >= TIMER_DELAY) {
            if (chip8.delayTimer > 0) chip8.delayTimer--;
            if (chip8.soundTimer > 0) chip8.soundTimer--;

            if (chip8.drawFlag) {
                uint32_t pixels[Chip8::WIDTH * Chip8::HEIGHT];

                for (int i = 0; i < Chip8::WIDTH * Chip8::HEIGHT; i++) {
                    pixels[i] = chip8.gfx[i] ? 0xFFFFFFFF : 0xFF000000;
                }

                SDL_UpdateTexture(texture, nullptr, pixels, Chip8::WIDTH * sizeof(uint32_t));
                SDL_RenderClear(renderer);

                SDL_RenderTexture(renderer, texture, nullptr, nullptr);
                SDL_RenderPresent(renderer);

                chip8.drawFlag = false;
            }

            lastTimerTick = now;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
