// Offline renders of each event text opcode (OP_01..OP_07, OP_09) on the
// Middlegate play-screen base frame. Writes PPM (+ PNG when Pillow is available).
//
// Usage: event_op_demo <data_dir> [output_dir]
//   data_dir   — repo root (map.dat, town.32, roster.dat, …)
//   output_dir — default: event_demos (created beside cwd, usually game/build/)

#include "mm2/GameState.h"
#include "mm2/events/EventTextView.h"
#include "mm2/events/ServiceSignResolver.h"
#include "mm2/events/ScriptedSceneEngine.h"
#include "mm2/gfx/AmigaPlayScreenLayout.h"
#include "mm2/gfx/EnvAssets.h"
#include "mm2/gfx/PlayScreenChrome.h"
#include "mm2/gfx/ScreenCompositor.h"
#include "mm2/gfx/OutdoorView3D.h"
#include "mm2/gfx/View3D.h"
#include "mm2/world/MapWorld.h"

#include "mm2_gamestate.h"
#include "mm2_gfx_sheet.h"
#include "mm2_party_launch.h"
#include "mm2_roster_codec.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

namespace {

using namespace mm2;
using namespace mm2::gfx;
using namespace mm2::gfx::play_layout;

static const char *g_demo_data_dir = nullptr;

struct PlayFrame;

struct DemoCase {
    const char *file_stem;
    const char *op_name;
    const char *source_note;
    void (*setup)(events::EventTextView &tv);
    void (*prepare_gs)(GameStateView &gs);  // nullptr → party-launch spawn coords
    void (*setup_with_frame)(events::EventTextView &tv, PlayFrame &frame);  // outdoor / attrib-aware
    events::ScriptedSceneId scripted_id = events::ScriptedSceneId::None;
};

void blitImageFrame(ScreenCompositor &c, const mm2_gfx_sheet &sheet, int frame, int dst_x, int dst_y)
{
    const mm2_image32_file &img = sheet.img;
    if (frame < 0 || frame >= img.frame_count) {
        return;
    }
    const mm2_image32_frame &f = img.frames[frame];
    if (!f.rgba) {
        return;
    }
    c.blitRgba(f.rgba, f.width, f.height, dst_x, dst_y);
}

struct PlayFrame {
    world::MapWorld world;
    EnvAssets env;
    ScreenCompositor compositor;
    GameStateView gs;
    uint8_t gs_image[static_cast<size_t>(MM2_A4_ANCHOR) + 0x8000u]{};
    Mm2PartyLaunch launch{};
    Mm2RosterFile roster{};
    bool ok = false;

    bool init(const char *data_dir)
    {
        char path[512];
        std::snprintf(path, sizeof(path), "%s/roster.dat", data_dir);
        if (mm2_roster_load_file(path, &roster) != MM2_ROSTER_OK) {
            return false;
        }

        int members[MM2_PARTY_LAUNCH_SLOTS];
        int count = 0;
        for (int i = 0; i < MM2_ROSTER_RECORD_COUNT && count < 6; ++i) {
            const Mm2RosterRecord &rec = roster.records[i];
            if (rec.name[0] != 0 && (rec.town_flags & 0x7F) == 1) {
                members[count++] = i;
            }
        }
        if (count == 0) {
            return false;
        }

        mm2_party_launch_build(&launch, 1, members, count);
        gs = GameStateView(mm2_gs_base_from_image(gs_image));
        gs.initCalendarDefaults();
        gs.initControlsDefaults();
        gs.initProtectDefaults();
        mm2_party_launch_apply(gs.a4(), &launch);

        mm2_image32_set_preview_opaque(0);
        if (!world.load(data_dir) || !world.enterScreen(0)) {
            return false;
        }
        if (!env.loadGlobal(data_dir) || !env.loadEnv(data_dir, envKindFromAttrib(world.attrib()))) {
            return false;
        }
        ok = true;
        return true;
    }

    void renderView3D()
    {
        View3DCamera camera{};
        camera.x = gs.coordX();
        camera.y = gs.coordY();
        camera.facing = gs.facing03();

        if (world.isOutdoor()) {
            blitImageFrame(compositor, env.floor(), 0, kView3DOriginX, kView3DFloorY);
            blitImageFrame(compositor, env.sky(), 0, kView3DOriginX, kView3DSkyY);
            const OutdoorScene scene = buildOutdoorScene(world, camera);
            for (int i = 0; i < scene.num_decor; ++i) {
                const OutdoorSpriteBlit &b = scene.decor[static_cast<size_t>(i)];
                blitImageFrame(compositor, env.biomeSheet(b.biome), b.frame, b.x, b.y);
            }
            for (int i = 0; i < scene.num_horizon; ++i) {
                const OutdoorSpriteBlit &b = scene.horizon[static_cast<size_t>(i)];
                blitImageFrame(compositor, env.horizonSheet(b.horizon), b.frame, b.x, b.y);
            }
            return;
        }

        const View3DMapBuffers bufs = world.buildView3DMapBuffers();
        const View3DScene scene = buildView3DScene(bufs, camera);
        const int sky_frame = world.roofBitAt(camera.x, camera.y) ? 1 : 0;
        blitImageFrame(compositor, env.floor(), 0, kView3DOriginX, kView3DFloorY);
        blitImageFrame(compositor, env.sky(), sky_frame, kView3DOriginX, kView3DSkyY);
        for (int i = 0; i < scene.num_blits; ++i) {
            const View3DBlit &b = scene.blits[static_cast<size_t>(i)];
            blitImageFrame(compositor, env.walls(), b.frame, b.x, b.y);
        }
    }

    void renderBase()
    {
        compositor.clear(0, 0, 0, 255);
        drawPlayScreenChrome(compositor);
        renderView3D();

        const bool protect_panel = gs.rightPanelMode() != 0;
        drawPlayStatusBar(compositor, gs.day(), gs.year(), gs.facingKey(), protect_panel);

        PlayPartySlot slots[8]{};
        for (int i = 0; i < launch.party_count && i < 8; ++i) {
            const int slot = launch.roster_slots[i];
            if (slot < 0 || slot >= MM2_ROSTER_RECORD_COUNT) {
                continue;
            }
            const Mm2RosterRecord &rec = roster.records[slot];
            slots[i].present = true;
            mm2_roster_name_to_cstr(&rec, slots[i].name, sizeof(slots[i].name));
            slots[i].hp = rec.hp_max;
            slots[i].bad_condition = rec.condition != 0;
        }
        drawPlayPartyPanel(compositor, slots);

        PlayProtectValues prot{};
        prot.light = gs.lightFactor();
        prot.magic = gs.magicProtect();
        prot.forces = gs.forcesProtect();
        prot.levitate = gs.levitateFlag();
        prot.walk_water = gs.walkWaterFlag();
        prot.guard_dog = gs.guardDogFlag();
        const PlayRightPanel panel =
            gs.rightPanelMode() == 0 ? PlayRightPanel::Options : PlayRightPanel::Protect;
        drawPlayRightColumn(compositor, panel, panel == PlayRightPanel::Protect ? &prot : nullptr);
    }
};

bool mkdir_p(const char *path)
{
#ifdef _WIN32
    return _mkdir(path) == 0 || errno == EEXIST;
#else
    return mkdir(path, 0755) == 0 || errno == EEXIST;
#endif
}

bool write_ppm(const char *path, const ScreenCompositor &c)
{
    std::FILE *f = std::fopen(path, "wb");
    if (!f) {
        return false;
    }
    std::fprintf(f, "P6\n%d %d\n255\n", ScreenCompositor::kWidth, ScreenCompositor::kHeight);
    const uint8_t *rgba = c.pixels();
    for (int i = 0; i < ScreenCompositor::kWidth * ScreenCompositor::kHeight; ++i) {
        std::fputc(rgba[i * 4 + 0], f);
        std::fputc(rgba[i * 4 + 1], f);
        std::fputc(rgba[i * 4 + 2], f);
    }
    std::fclose(f);
    return true;
}

void try_ppm_to_png(const char *ppm_path)
{
    char cmd[768];
    std::snprintf(cmd, sizeof(cmd),
                  "python -c \"from PIL import Image; "
                  "p=r'%s'; Image.open(p).save(p[:-4]+'.png')\" 2>nul",
                  ppm_path);
    std::system(cmd);
}

void setup_op01(events::EventTextView &tv)
{
    tv.showOp01("You are at the city gates. Exit (y/n)?");
}

void setup_op02(events::EventTextView &tv)
{
    tv.showOp02("Drink from the fountain of\nclairvoyance (y/n)?");
}

void setup_op03(events::EventTextView &tv)
{
    tv.showOp03("Suddenly your party is moving very\nslowly, yet everything around them\n"
                "appears to move at normal speed.");
}

void setup_op04(events::EventTextView &tv)
{
    tv.showOp04("Middlegate Inn");
}

void prepare_op04_gs(GameStateView &gs)
{
    /* Event 01 trigger (7,5)/DIR_N? — approach from north, face S toward inn door on (7,4). */
    gs.setCoordX(7);
    gs.setCoordY(5);
    gs.setFacingKey('S');
}

void prepare_scripted_corak_gs(GameStateView &gs)
{
    gs.setRightPanelMode(0); /* OPTIONS panel @ A4-$79B2 = 0 */
}

void prepare_scripted_pegasus_gs(GameStateView &gs)
{
    gs.setScreenId(11);
    gs.setCoordY(4);
    gs.setCoordX(7);
    gs.setFacingKey('N');
    gs.setRightPanelMode(1); /* Protect panel */
}

void setup_op05(events::EventTextView &tv)
{
    tv.showOp05("Monster Freezing\nAuthorized Personnel\nOnly!");
}

void prepare_op06_gs(GameStateView &gs)
{
    /* loc 10 (B2) evt 02 @ (1,11)/ENTER — outdoor signpost (doc 44 §6.3). */
    gs.setScreenId(10);
    gs.setCoordX(1);
    gs.setCoordY(11);
    gs.setFacingKey('N');
}

void setup_op06(events::EventTextView &tv)
{
    tv.showOp06("Archers\nOnly");
}

void setup_op0b(events::EventTextView &tv)
{
    /* loc 00 evt 22 @ (4,4): OP_0B str[2] pos 0 -> 62.anm S.J. Blacksmith (0x15756 table). */
    tv.showOp0B("S.J. Blacksmith", g_demo_data_dir, 0, nullptr, 2, 0);
}

void setup_op0b_portcullis(events::EventTextView &tv, PlayFrame &frame)
{
    /* loc 11 evt 01 @ (3,7)/DIR_S: OP_0B str[24] -> 29.anm (table id 29, env 3) + OP_02 + OP_09. */
    frame.gs.setScreenId(11);
    frame.gs.setCoordY(3);
    frame.gs.setCoordX(7);
    frame.gs.setFacingKey('S');
    frame.world.enterScreen(11);
    frame.env.loadEnv(g_demo_data_dir, envKindFromAttrib(frame.world.attrib()));
    events::ServiceSignResolver::syncSignEnvId(frame.gs.a4(), 11, &frame.world.attrib());
    tv.showOp0B(nullptr, g_demo_data_dir, frame.gs, &frame.world.attrib(), 24, 0);
    tv.showOp02("A giant portcullis slowly raises,\noffering admittance to Middlegate.\n"
                "Enter (y/n)?");
}

void setup_op0b_hillstone(events::EventTextView &tv, PlayFrame &frame)
{
    /* loc 39 evt 02 @ (1,13)/ENTER: OP_0B str[6] -> table 47, file 47.anm portrait + OP_02 marble walls. */
    frame.gs.setScreenId(39);
    frame.gs.setCoordY(1);
    frame.gs.setCoordX(13);
    frame.gs.setFacingKey('N');
    frame.world.enterScreen(39);
    frame.env.loadEnv(g_demo_data_dir, envKindFromAttrib(frame.world.attrib()));
    events::ServiceSignResolver::syncSignEnvId(frame.gs.a4(), 39, &frame.world.attrib());
    tv.showOp0B(nullptr, g_demo_data_dir, frame.gs, &frame.world.attrib(), 6, 0);
    tv.showOp02("The marble walls of Castle Hillstone\ngleam before you. Enter (y/n)?");
}

void setup_op07(events::EventTextView &tv)
{
    tv.showOp02("You have found an N-19 Capitor.");
    tv.showSpacePrompt();
}

void setup_op09(events::EventTextView &tv)
{
    tv.showOp01("You are at the city gates. Exit (y/n)?");
}

constexpr DemoCase kDemos[] = {
    {"op01_center_row17", "OP_01", "Middlegate event 20 str[24] city gates", setup_op01, nullptr, nullptr},
    {"op02_block_rows19_22", "OP_02", "Middlegate event 42 str[9] fountain", setup_op02, nullptr, nullptr},
    {"op03_tall_block_rows17_22", "OP_03", "Pinehurst event 26 str[23] slow-time", setup_op03, nullptr,
     nullptr},
    {"op04_door_label_row3", "OP_04", "Middlegate event 01 @ (7,5)/S str[1] Middlegate Inn", setup_op04,
     prepare_op04_gs, nullptr},
    {"op05_popup_a", "OP_05", "Tundara str[23] Monster Freezing popup", setup_op05, nullptr, nullptr},
    {"op06_signpost", "OP_06", "B2 event 02 str[1] Archers Only (outdoor signpost)", setup_op06,
     prepare_op06_gs, nullptr},
    {"op0b_signboard", "OP_0B", "Middlegate evt 22 str[2] 62.anm S.J. Blacksmith @ (4,4)", setup_op0b,
     nullptr, nullptr},
    {"op0b_portcullis", "OP_0B", "C2 loc 11 evt 01 str[24] 29.anm gate portrait @ (3,7)/S", nullptr,
     nullptr, setup_op0b_portcullis},
    {"op0b_hillstone_castle", "OP_0B", "D4 loc 39 evt 02 str[6] 47.anm Hillstone portrait @ (1,13)/ENTER",
     nullptr, nullptr, setup_op0b_hillstone},
    {"op07_space_prompt", "OP_07", "Hillstone str[14] loot + SPACE prompt", setup_op07, nullptr, nullptr},
    {"op09_yn_prompt", "OP_09", "Middlegate event 20 — Y/N input only (no extra draw)", setup_op09, nullptr,
     nullptr},
    {"scripted_corak", "ScriptedScene", "Corak ghost overlay + loc 60 str[1] + SPACE (doc 46)", nullptr,
     prepare_scripted_corak_gs, nullptr, events::ScriptedSceneId::CorakIntro},
    {"scripted_pegasus", "ScriptedScene", "C2 outdoor @ (4,7) + 21.anm (#131) + loc 11 str[5] OP_03 (doc 46)",
     nullptr, prepare_scripted_pegasus_gs, nullptr, events::ScriptedSceneId::PegasusC2},
};

}  // namespace

int main(int argc, char **argv)
{
    const char *data_dir = (argc > 1) ? argv[1] : "../..";
    const char *out_dir = (argc > 2) ? argv[2] : "event_demos";

    if (!mkdir_p(out_dir)) {
        std::fprintf(stderr, "FAIL: cannot create output dir %s\n", out_dir);
        return 1;
    }

    g_demo_data_dir = data_dir;

    PlayFrame frame;
    if (!frame.init(data_dir)) {
        std::fprintf(stderr, "FAIL: init play frame from %s\n", data_dir);
        return 1;
    }

    events::ScriptedSceneEngine scripted;
    const bool has_scripted = scripted.load(data_dir);

    std::printf("Event text opcode demos -> %s/\n", out_dir);
    for (const DemoCase &demo : kDemos) {
        events::EventTextView tv;
        if (demo.prepare_gs) {
            demo.prepare_gs(frame.gs);
        } else {
            mm2_party_launch_apply(frame.gs.a4(), &frame.launch);
        }

        if (demo.prepare_gs == prepare_op06_gs) {
            frame.world.enterScreen(10);
            frame.env.loadEnv(data_dir, envKindFromAttrib(frame.world.attrib()));
        }

        if (demo.setup_with_frame) {
            demo.setup_with_frame(tv, frame);
        } else if (demo.setup) {
            demo.setup(tv);
        }

        if (demo.scripted_id == events::ScriptedSceneId::PegasusC2) {
            frame.world.enterScreen(11);
            frame.env.loadEnv(data_dir, envKindFromAttrib(frame.world.attrib()));
        }

        frame.renderBase();

        /* Advance multi-frame .anm overlays (~80 game ticks) before capture. */
        for (int t = 0; t < 80; ++t) {
            if (demo.scripted_id != events::ScriptedSceneId::None && has_scripted) {
                scripted.tickAnimation();
            } else if (demo.setup || demo.setup_with_frame) {
                tv.tickAnimation();
            }
        }

        if (demo.scripted_id != events::ScriptedSceneId::None && has_scripted) {
            scripted.armDemo(demo.scripted_id);
            scripted.draw(frame.compositor);
        } else if (demo.setup || demo.setup_with_frame) {
            tv.draw(frame.compositor);
        }

        char ppm_path[512];
        std::snprintf(ppm_path, sizeof(ppm_path), "%s/%s.ppm", out_dir, demo.file_stem);
        if (!write_ppm(ppm_path, frame.compositor)) {
            std::fprintf(stderr, "FAIL: write %s\n", ppm_path);
            return 1;
        }
        try_ppm_to_png(ppm_path);
        std::printf("  %s  (%s)  %s\n", demo.file_stem, demo.op_name, demo.source_note);
    }

    char readme_path[512];
    std::snprintf(readme_path, sizeof(readme_path), "%s/README.md", out_dir);
    std::FILE *readme = std::fopen(readme_path, "w");
    if (readme) {
        std::fprintf(readme, "# Event text opcode demos\n\n");
        std::fprintf(readme, "Generated by `event_op_demo` on the Middlegate play-screen base frame "
                             "(default inn spawn 7,3 N; OP_04 uses event-01 trigger 7,5 S).\n\n");
        std::fprintf(readme, "| File | Opcode | Source |\n");
        std::fprintf(readme, "|------|--------|--------|\n");
        for (const DemoCase &demo : kDemos) {
            std::fprintf(readme, "| `%s.ppm` / `.png` | **%s** | %s |\n", demo.file_stem, demo.op_name,
                         demo.source_note);
        }
        std::fprintf(readme, "\n## Scripted scenes (doc 46)\n\n");
        std::fprintf(readme, "| File | Scene | Source |\n");
        std::fprintf(readme, "|------|-------|--------|\n");
        std::fprintf(readme, "| `scripted_corak` | Corak intro | loc 60 str[1], ghost 51.anm candidate |\n");
        std::fprintf(readme, "| `scripted_pegasus` | Pegasus C2 | 21.anm over outdoor 3D, loc 11 str[5] OP_03 @ (4,7) |\n");
        std::fprintf(readme, "\n## Not rendered\n\n");
        std::fprintf(readme, "- **OP_08** / **OP_0A** — scripted-input variants of OP_07 / OP_09; "
                             "no distinct draw path.\n");
        std::fprintf(readme, "- **OP_0B** `op0b_signboard` = Middlegate blacksmith `62.anm`; "
                             "`op0b_portcullis` = C2 gate `29.anm` (loc 11 evt 01 str[24]); "
                             "`op0b_hillstone_castle` = D4 `47.anm` (loc 39 evt 02 str[6]).\n");
        std::fprintf(readme, "\nRegenerate: `cmake --build build && ./build/event_op_demo.exe ..`\n");
        std::fclose(readme);
    }

    std::printf("OK: %zu screenshots in %s/\n", sizeof(kDemos) / sizeof(kDemos[0]), out_dir);
    return 0;
}
