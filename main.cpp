#include <fstream>
#include <iostream>
#include <random>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_events.h>

using u8 = uint8_t;
using u16 = uint16_t;

#define PIXEL_SIZE 10

struct Chip8 {
    u8 memory[4096]{};
    u8 V[16]{};
    u16 I = 0;
    u16 PC = 0x200;
    u8 delayTimer = 0;
    u8 soundTimer = 0;
    u16 stack[16]{};
    u8 SP = 0;

    static constexpr int WIDTH = 64;
    static constexpr int HEIGHT = 32;
    u8 gfx[WIDTH * HEIGHT]{};

    u8 keys[16]{};

    std::mt19937 rng{std::random_device{}()};

    bool drawFlag = false;

    bool loadROM(const std::string &path) {
        std::ifstream file{path, std::ios::binary};
        if (!file) return false;

        std::vector<u8> buffer((std::istreambuf_iterator(file)), std::istreambuf_iterator<char>());

        std::cout << "ROM size: " << buffer.size() << " bytes\n";

        if (buffer.size() + 0x200 > sizeof(memory)) return false;

        std::memcpy(memory + 0x200, buffer.data(), buffer.size());
        return true;
    }

    void step() {
        int i = 0;

        const u16 opcode = memory[PC] << 8 | memory[PC + 1];

        const u8 x = opcode >> 8 & 0x000F;
        const u8 y = opcode >> 4 & 0x000F;
        const u8 n = opcode & 0x000F;
        const u8 nn = opcode & 0x00FF;
        const u16 nnn = opcode & 0x0FFF;

        u16 nextPC = PC + 2;

        // printf("PC: 0x%04x Op: 0x%04x\n", PC, opcode);

        switch (opcode & 0xF000) {
            case 0x0000:
                switch (nn) {
                    case 0x00E0:
                        // TODO
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
                switch (n) {
                    case 0x0:
                        nextPC = V[x] != V[y] ? PC + 4 : PC + 2;
                        break;
                    default:
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
                    case 0x0A:
                        i = 0;
                        while (true) {
                            for (i = 0; i < 16; i++) {
                                if (keys[i]) {
                                    V[x] = i;
                                    goto got_key_press;
                                }
                            }
                        }
                    got_key_press:
                        break;
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
                        memory[I] = V[x] % 1000 / 100;
                        memory[I + 1] = V[x] % 100 / 10;
                        memory[I + 2] = V[x] % 10;
                        break;
                    case 0x55:
                        for (i = 0; i <= x; i++) { memory[I + i] = V[i]; }
                        I += x + 1;
                        break;
                    case 0x65:
                        for (i = 0; i <= x; i++) { V[i] = memory[I + i]; }
                        I += x + 1;
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

    void drawSprite(const u8 xPos, const u8 yPos, const u8 height) {
        V[0xF] = 0;

        for (int row = 0; row < height; row++) {
            u8 spriteByte = memory[I + row];

            for (int col = 0; col < 8; col++) {
                if ((spriteByte & 0x80 >> col) != 0) {
                    int x = (xPos + col) % WIDTH;
                    int y = (yPos + row) % HEIGHT;
                    int index = y * WIDTH + x;

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


    static void unknownOpcode(const u16 opcode) {
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

    bool running = true;

    constexpr int CPU_HZ = 400;
    constexpr float CPU_MS_PER_STEP = 1000.0f / CPU_HZ;

    float cpuTimer = 0.0f;

    constexpr int TIMER_HZ = 60;
    constexpr float TIMER_MS_PER_STEP = 1000.0f / TIMER_HZ;
    float timerAccumulator = 0.0f;

    Uint32 lastTicks = SDL_GetTicks();

    while (running) {
        const Uint32 now = SDL_GetTicks();
        const auto delta = static_cast<float>(now - lastTicks);
        lastTicks = now;

        cpuTimer += delta;
        timerAccumulator += delta;

        while (timerAccumulator >= TIMER_MS_PER_STEP) {
            if (chip8.delayTimer > 0) chip8.delayTimer--;
            if (chip8.soundTimer > 0) chip8.soundTimer--;
            timerAccumulator -= TIMER_MS_PER_STEP;
        }

        while (cpuTimer >= CPU_MS_PER_STEP) {
            chip8.step();
            cpuTimer -= CPU_MS_PER_STEP;
        }

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) running = false;

            if (e.type == SDL_EVENT_KEY_DOWN) {
                if (const int k = Chip8::mapSDLKeyToChip8(e.key.scancode); k != -1) chip8.keys[k] = 1;
            }
            if (e.type == SDL_EVENT_KEY_UP) {
                if (const int k = Chip8::mapSDLKeyToChip8(e.key.scancode); k != -1) chip8.keys[k] = 0;
            }
        }

        if (chip8.drawFlag) {
            uint32_t pixels[Chip8::WIDTH * Chip8::HEIGHT];

            for (int i = 0; i < Chip8::WIDTH * Chip8::HEIGHT; i++) {
                pixels[i] = chip8.gfx[i] ? 0xFFFFFFFF : 0xFF000000;
            }

            SDL_UpdateTexture(texture, nullptr, pixels, Chip8::WIDTH * sizeof(uint32_t));
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);

            SDL_FRect dst = {0, 0, Chip8::WIDTH * PIXEL_SIZE, Chip8::HEIGHT * PIXEL_SIZE};
            SDL_RenderTexture(renderer, texture, nullptr, &dst);

            SDL_RenderPresent(renderer);
            chip8.drawFlag = false;
        }
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
