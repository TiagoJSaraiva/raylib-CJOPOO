#include "wall_prop_renderer.h"
#include "wall_tiles.h"
#include "rlgl.h"

#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>

namespace {
namespace fs = std::filesystem;
const std::array<Color, 8> colors{RED, GREEN, BLUE, YELLOW, PURPLE, ORANGE, PINK, SKYBLUE};
RenderTexture2D target;

bool Near(int a, int b, int tolerance = 4) { return std::abs(a - b) <= tolerance; }
void Expect(Color actual, Color expected) {
    assert(Near(actual.r, expected.r) && Near(actual.g, expected.g) && Near(actual.b, expected.b));
}
Color Over(Color top, Color bottom, float opacity) {
    return {static_cast<unsigned char>(top.r * opacity + bottom.r * (1 - opacity)),
            static_cast<unsigned char>(top.g * opacity + bottom.g * (1 - opacity)),
            static_cast<unsigned char>(top.b * opacity + bottom.b * (1 - opacity)), 255};
}
Image Capture() {
    EndTextureMode();
    Image image = LoadImageFromTexture(target.texture);
    ImageFlipVertical(&image);
    return image;
}
Room MakeRoom(RoomCoords coords = {}, TileRect bounds = {2, 3, 10, 6}, RoomType type = RoomType::Normal) {
    RoomLayout layout;
    layout.tileBounds = bounds; layout.widthTiles = bounds.width; layout.heightTiles = bounds.height;
    return Room(coords, {type, BiomeType::Dungeon, 17}, layout);
}
WallPropInstance At(const Room& room, WallRules::Section section, const WallPropDefinition& definition) {
    const auto modules = WallRules::BuildModules(WallRules::BuildFrame({{&room, 1}}));
    for (const auto& module : modules) if (module.segment.section == section && module.widthTiles == 2)
        return {{module.segment.host, section, module.segment.position}, &definition, module, 0};
    assert(false); return {};
}

struct TemporaryAssets {
    fs::path original = fs::current_path();
    fs::path root = fs::temp_directory_path() /
        ("endless-wall-props-" + std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count()));
    TemporaryAssets() {
        for (const char* biome : {"lobby", "cave", "dungeon", "mansion"})
            for (const char* section : {"upper", "bottom", "side", "edge"})
                fs::create_directories(root / "assets/wall" / biome / section);
        fs::create_directories(root / "assets/img/wallprop");
        fs::current_path(root);
        Image wall = GenImageColor(16, 16, BLUE);
        assert(ExportImage(wall, "assets/wall/dungeon/upper/1-1.png"));
        assert(ExportImage(wall, "assets/wall/dungeon/bottom/1-1.png"));
        UnloadImage(wall);
        Image sheet = GenImageColor(128, 16, BLANK);
        for (int i = 0; i < 8; ++i) ImageDrawRectangle(&sheet, i * 16 + 4, 0, 8, 16, colors[i]);
        assert(ExportImage(sheet, "assets/img/wallprop/test.png")); UnloadImage(sheet);
    }
    ~TemporaryAssets() { fs::current_path(original); fs::remove_all(root); }
};

void SpritesAndOcclusion() {
    TemporaryAssets assets;
    auto room = MakeRoom();
    auto definition = GetWallPropDefinitions()[0];
    definition.texturePath = "assets/img/wallprop/test.png";
    definition.animation = PropAnimation{16, 16, 8, 0.1f, false};
    const auto upper = At(room, WallRules::Section::Upper, definition);
    const auto bottom = At(room, WallRules::Section::Bottom, definition);
    PropRenderer renderer;
    WallTiles walls; walls.BeginRun(1);
    const std::vector<WallRules::RoomView> views{{&room, 1}};
    const Rectangle b = bottom.Bounds();
    const int x = static_cast<int>(b.x + 64), y = static_cast<int>(b.y + 64);
    for (int frame = 0; frame < 8; ++frame) {
        const float time = frame * 0.1f + 0.01f;
        const auto g = PropRenderer::SpriteGeometry(upper.Bounds(), definition.animation, 128, 16, time);
        assert(g.source.x == frame * 16 && g.source.width == 16 && g.source.height == 16);
        assert(g.destination.width == 128 && g.destination.y + g.destination.height == upper.Depth());
        BeginTextureMode(target); ClearBackground(WHITE); walls.DrawBackground(views);
        WallPropRendering::Draw({&upper, 1}, renderer, time);
        Image image = Capture();
        Expect(GetImageColor(image, static_cast<int>(upper.Bounds().x + 64), static_cast<int>(upper.Bounds().y + 64)), colors[frame]);
        // Transparent margins remain part of the fit.
        Expect(GetImageColor(image, static_cast<int>(upper.Bounds().x + 8), static_cast<int>(upper.Bounds().y + 64)), BLUE);
        UnloadImage(image);
    }
    auto render = [&](bool torch, Rectangle player, float delta, float actorDepth = -1.0f) {
        BeginTextureMode(target); ClearBackground(WHITE); walls.DrawBackground(views);
        std::vector<SceneActor> actors;
        if (torch) actors.push_back({bottom.Depth(), [&] { WallPropRendering::Draw({&bottom, 1}, renderer, 0); }});
        if (actorDepth >= 0) actors.push_back({actorDepth, [=] { DrawRectangle(x - 8, y - 8, 16, 16, GREEN); }});
        DrawDepthSorted(actors);
        walls.DrawForeground(views, player, delta);
        return Capture();
    };
    const Rectangle away{800, 700, 10, 10}, overlapping{b.x + 24, b.y + 24, 24, 24};
    Image absent = render(false, away, 0.2f);
    Image opaque = render(true, away, 0.2f);
    for (int dy = 0; dy < 128; ++dy) for (int dx = 0; dx < 128; ++dx)
        Expect(GetImageColor(opaque, int(b.x) + dx, int(b.y) + dy), GetImageColor(absent, int(b.x) + dx, int(b.y) + dy));
    UnloadImage(absent); UnloadImage(opaque);
    Image entering = render(true, overlapping, 0.1f);
    Expect(GetImageColor(entering, x, y), Over(BLUE, RED, 0.8f)); UnloadImage(entering);
    Image faded = render(true, overlapping, 0.1f);
    Expect(GetImageColor(faded, x, y), Over(BLUE, RED, 0.6f)); UnloadImage(faded);
    Image actorBehind = render(true, overlapping, 0, bottom.Depth() - 1);
    Expect(GetImageColor(actorBehind, x, y), Over(BLUE, RED, 0.6f)); UnloadImage(actorBehind);
    Image actorInFront = render(true, overlapping, 0, bottom.Depth() + 1);
    Expect(GetImageColor(actorInFront, x, y), Over(BLUE, GREEN, 0.6f)); UnloadImage(actorInFront);
    Image leaving = render(true, away, 0.1f);
    Expect(GetImageColor(leaving, x, y), Over(BLUE, RED, 0.8f)); UnloadImage(leaving);
    Image restored = render(true, away, 0.1f);
    Expect(GetImageColor(restored, x, y), BLUE); UnloadImage(restored);
    renderer.Unload(); renderer.Unload();
}

void Lighting() {
    TemporaryAssets assets;
    auto room = MakeRoom();
    auto definition = GetWallPropDefinitions()[0];
    definition.texturePath = "assets/img/wallprop/test.png";
    definition.animation = PropAnimation{16, 16, 8, 0.1f, false};
    const auto bottom = At(room, WallRules::Section::Bottom, definition);
    PropRenderer renderer; SceneLighting lighting;
    Camera2D camera{}; camera.zoom = 1;
    const auto sources = WallPropRendering::Lights({{&bottom, 1}}, renderer, 0);
    assert(sources.size() == 1);
    assert(sources[0].light.radius > PropLight{}.radius && sources[0].light.intensity > PropLight{}.intensity);
    const Vector2 p = sources[0].position;
    auto render = [&](const std::vector<SceneLightSource>& lights, float visibility = 1.0f) {
        BeginTextureMode(target); ClearBackground(WHITE);
        lighting.Apply({{&room, visibility}}, camera, renderer, 0, &target, lights);
        return Capture();
    };
    Image baseline = render({}); Image torch = render(sources);
    auto lit = GetImageColor(torch, int(p.x + 220), int(p.y));
    auto dark = GetImageColor(baseline, int(p.x + 220), int(p.y));
    assert(lit.r > dark.r + 8);
    auto candleSources = sources; candleSources[0].light = PropLight{};
    Image candle = render(candleSources);
    Expect(GetImageColor(candle, int(p.x + 220), int(p.y)), dark);
    // The source is emitted even though its sprite would be hidden by an opaque bottom.
    assert(GetImageColor(torch, int(p.x), int(p.y)).r > GetImageColor(baseline, int(p.x), int(p.y)).r + 40);
    const int outside = int(bottom.LightClip().x - 1);
    Expect(GetImageColor(torch, outside, int(p.y)), GetImageColor(baseline, outside, int(p.y)));
    UnloadImage(baseline); UnloadImage(torch); UnloadImage(candle);
    camera.zoom = 0.5f; camera.offset = {20, 30};
    baseline = render({}); torch = render(sources);
    Vector2 sample = GetWorldToScreen2D({p.x + 220, p.y}, camera);
    assert(GetImageColor(torch, int(sample.x), int(sample.y)).r > GetImageColor(baseline, int(sample.x), int(sample.y)).r + 8);
    UnloadImage(baseline); UnloadImage(torch);
    camera.zoom = 1; camera.offset = {};
    assert(WallPropRules::VisibleInstances({bottom}, {{&room, 0}}).empty());
    torch = render({}, 0); Expect(GetImageColor(torch, int(p.x), int(p.y)), WHITE); UnloadImage(torch);

    auto left = MakeRoom({}, {0, 3, 6, 6}, RoomType::Shop);
    auto right = MakeRoom({1, 0}, {10, 3, 6, 6}, RoomType::Shop);
    Doorway east; east.direction = Direction::East; east.offset = 2; east.width = 2;
    east.corridorTiles = {6, 5, 4, 2}; east.targetCoords = right.GetCoords();
    east.doorState = std::make_shared<DoorInstance>();
    Doorway west = east; west.direction = Direction::West; west.targetCoords = left.GetCoords();
    left.Layout().doors.push_back(east); right.Layout().doors.push_back(west);
    const std::vector<WallPropDefinition> catalog{definition};
    const auto props = WallPropRules::Generate({{&left, 1}, {&right, 1}}, 1, catalog, {1, 2});
    assert(props.size() == 2);
    auto wallViews = WallPropRules::VisibleInstances(props, {{&left, 1}, {&right, 1}});
    assert(WallPropRendering::Lights(wallViews, renderer, 0).empty());
    east.doorState->open = true;
    wallViews = WallPropRules::VisibleInstances(props, {{&left, 1}, {&right, 1}});
    const auto corridorLights = WallPropRendering::Lights(wallViews, renderer, 0);
    assert(corridorLights.size() == 2);
    auto renderCorridor = [&](const std::vector<SceneLightSource>& lights) {
        BeginTextureMode(target); ClearBackground(WHITE);
        lighting.Apply({{&left, 1}, {&right, 1}}, camera, renderer, 0, &target, lights);
        return Capture();
    };
    baseline = renderCorridor({}); torch = renderCorridor(corridorLights);
    const Vector2 cp = corridorLights.front().position;
    assert(GetImageColor(torch, int(cp.x), int(cp.y)).r > GetImageColor(baseline, int(cp.x), int(cp.y)).r + 40);
    Expect(GetImageColor(torch, 6 * TILE_SIZE - 1, int(cp.y)), GetImageColor(baseline, 6 * TILE_SIZE - 1, int(cp.y)));
    Expect(GetImageColor(torch, 10 * TILE_SIZE, int(cp.y)), GetImageColor(baseline, 10 * TILE_SIZE, int(cp.y)));
    UnloadImage(baseline); UnloadImage(torch);
    lighting.Unload(); lighting.Unload();
}

void RealAssetOcclusion() {
    auto room = MakeRoom();
    const auto& definition = GetWallPropDefinitions()[0];
    const auto prop = At(room, WallRules::Section::Bottom, definition);
    const auto b = prop.Bounds();
    PropRenderer renderer; WallTiles walls; walls.BeginRun(17);
    auto render = [&](bool torch, bool translucent) {
        BeginTextureMode(target); ClearBackground(WHITE);
        walls.DrawBackground({{&room, 1}});
        if (torch) WallPropRendering::Draw({&prop, 1}, renderer, 0.23f);
        const Rectangle player = translucent ? Rectangle{b.x + 32, b.y + 32, 24, 24} : Rectangle{2000, 2000, 24, 24};
        walls.DrawForeground({{&room, 1}}, player, 0.2f);
        return Capture();
    };
    Image without = render(false, false), opaque = render(true, false);
    for (int y = int(b.y); y < int(b.y + b.height); ++y)
        for (int x = int(b.x); x < int(b.x + b.width); ++x)
            Expect(GetImageColor(opaque, x, y), GetImageColor(without, x, y));
    UnloadImage(without); UnloadImage(opaque);
    without = render(false, true); Image faded = render(true, true);
    int revealedPixels = 0;
    for (int y = int(b.y); y < int(b.y + b.height); ++y)
        for (int x = int(b.x); x < int(b.x + b.width); ++x) {
            const auto a = GetImageColor(without, x, y), c = GetImageColor(faded, x, y);
            if (!Near(a.r, c.r) || !Near(a.g, c.g) || !Near(a.b, c.b)) ++revealedPixels;
        }
    assert(revealedPixels > 20);
    UnloadImage(without); UnloadImage(faded);
}

void Preview() {
    PropRenderer renderer; SceneLighting lighting; WallTiles walls;
    walls.BeginRun(17);
    auto room = MakeRoom({}, {1, 3, 6, 4});
    const auto& definition = GetWallPropDefinitions()[0];
    const auto upper = At(room, WallRules::Section::Upper, definition);
    const auto bottom = At(room, WallRules::Section::Bottom, definition);
    auto left = MakeRoom({}, {0, 3, 4, 4}, RoomType::Shop);
    auto right = MakeRoom({1, 0}, {8, 3, 4, 4}, RoomType::Shop);
    Doorway east; east.direction = Direction::East; east.offset = 1; east.width = 2;
    east.corridorTiles = {4, 4, 4, 2}; east.targetCoords = right.GetCoords();
    east.doorState = std::make_shared<DoorInstance>(); east.doorState->open = true;
    Doorway west = east; west.direction = Direction::West; west.targetCoords = left.GetCoords();
    left.Layout().doors.push_back(east); right.Layout().doors.push_back(west);
    const auto corridorProps = WallPropRules::Generate({{&left, 1}, {&right, 1}}, 17, GetWallPropDefinitions(), {1, 2});
    const auto original = target;
    target = LoadRenderTexture(512, 384);
    Image preview = GenImageColor(1024, 768, BLANK);
    // Independent passes avoid multiplying previous panels when applying scene lighting.
    for (int panel = 0; panel < 4; ++panel) {
        const int px = (panel % 2) * 512, py = (panel / 2) * 384;
        Camera2D camera{}; camera.zoom = 1; camera.offset = {40, 50};
        camera.target = panel == 3 ? Vector2{192, 128} : Vector2{64, panel == 0 ? 32.0f : 256.0f};
        std::vector<WallRules::RoomView> rooms = panel == 3
            ? std::vector<WallRules::RoomView>{{&left, 1}, {&right, 1}}
            : std::vector<WallRules::RoomView>{{&room, 1}};
        std::vector<FloorRoomView> floors;
        for (const auto& view : rooms) floors.push_back({view.room, view.visibility});
        const std::vector<WallPropView> props = panel == 3
            ? WallPropRules::VisibleInstances(corridorProps, rooms)
            : std::vector<WallPropView>{{panel == 0 ? &upper : &bottom, 1}};
        BeginTextureMode(target); ClearBackground({24, 26, 33, 255});
        BeginMode2D(camera);
        for (const auto& view : rooms) {
            const auto b = view.room->Layout().tileBounds;
            for (int y = b.y; y < b.y + b.height; ++y) for (int x = b.x; x < b.x + b.width; ++x)
                DrawRectangle(x * 64, y * 64, 63, 63, {75, 75, 70, 255});
        }
        if (panel == 3) DrawRectangle(256, 256, 256, 128, {75, 75, 70, 255});
        walls.DrawBackground(rooms);
        for (const auto& view : props) WallPropRendering::Draw(view, renderer, 0.23f);
        const Rectangle player = panel == 2 ? Rectangle{128, 360, 24, 24} : Rectangle{2000, 2000, 24, 24};
        walls.DrawForeground(rooms, player, 0.2f);
        EndMode2D();
        const auto lights = WallPropRendering::Lights(props, renderer, 0.23f);
        lighting.Apply(floors, camera, renderer, 0.23f, &target, lights);
        DrawText(panel == 0 ? "UPPER" : panel == 1 ? "BOTTOM OPACA" : panel == 2 ? "BOTTOM TRANSLUCIDA" : "CORREDOR", 18, 14, 20, WHITE);
        Image image = Capture();
        ImageDraw(&preview, image, {0, 0, 512, 384}, {float(px), float(py), 512, 384}, WHITE);
        UnloadImage(image);
    }
    fs::create_directories("obj");
    assert(ExportImage(preview, "obj/wall-props-preview.png")); UnloadImage(preview);
    UnloadRenderTexture(target); target = original;
    lighting.Unload(); renderer.Unload();
}
}

int main() {
    SetTraceLogLevel(LOG_WARNING); SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(1024, 768, "Wall prop render tests");
    assert(IsWindowReady()); target = LoadRenderTexture(1024, 768);
    SpritesAndOcclusion(); Lighting(); RealAssetOcclusion(); Preview();
    UnloadRenderTexture(target); CloseWindow();
    std::cout << "Wall prop render tests passed\n";
}
