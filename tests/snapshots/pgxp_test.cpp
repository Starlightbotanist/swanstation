#include "../../src/core/pgxp.cpp"
#include <cstdio>
#include <new>

Settings::Settings() = default;
Settings g_settings;
namespace Bus { uint32_t g_ram_mask = RAM_2MB_SIZE - 1; }
static int fail_after = -1;
static unsigned allocation_hits;
void* operator new(std::size_t bytes)
{
  if (fail_after == 0)
  {
    allocation_hits++;
    throw std::bad_alloc();
  }
  if (fail_after > 0)
    fail_after--;
  void* p = std::malloc(bytes ? bytes : 1);
  if (!p)
    throw std::bad_alloc();
  return p;
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

static unsigned checks;
#define CHECK(condition) do { checks++; if (!(condition)) { std::fprintf(stderr,"PGXP:%d: %s\n",__LINE__,#condition);std::exit(1); } } while (0)

static const uint32_t packed = 12 | (8 << 16);
static void write_vertex(float x, float y)
{
  PGXP::GTE_PushSXYZ2f(x, y, 32768.0f, packed);
  PGXP::CPU_MFC2((2u << 16) | (14u << 11), packed, packed);
  PGXP::CPU_SW(2u << 16, packed, 0x2000);
}

static std::array<float, 3> read_vertex(uint32_t address)
{
  std::array<float, 3> result;
  const bool depth_valid = PGXP::GetPreciseVertex(address, packed, 12, 8, 0, 0, &result[0], &result[1], &result[2]);
  CHECK(depth_valid == (address != 0xffffffff));
  CHECK(result[0] != 12.0f);
  return result;
}

int main(int argc, char** argv)
{
  for (bool cache : {false, true})
  {
    if (cache && argc == 2 && !std::strcmp(argv[1],"--no-cache"))
      continue;
    g_settings.gpu_pgxp_enable = true;
    g_settings.gpu_pgxp_vertex_cache = cache;
    g_settings.gpu_pgxp_tolerance = -1.0f;
    PGXP::Initialize();
    write_vertex(12.125f, 8.25f);
    const auto expected = read_vertex(0x2000);
    const auto expected_cache = cache ? read_vertex(0xffffffff) : expected;
    std::shared_ptr<const PGXP::MemorySnapshot> anchor, other;
    CHECK(PGXP::SaveMemorySnapshot(&anchor));
    CHECK(PGXP::MemorySnapshotMatches(anchor));
    CHECK(anchor->memory[0] == anchor->memory[1]);
    if (cache)
      CHECK(anchor->vertices[0] == anchor->vertices.back());
    write_vertex(12.75f, 8.875f);
    CHECK(read_vertex(0x2000) != expected);
    CHECK(PGXP::SaveMemorySnapshot(&other));
    CHECK(other->memory[0x2000 / 4 / PGXP::SNAPSHOT_PAGE_VALUES] !=
          anchor->memory[0x2000 / 4 / PGXP::SNAPSHOT_PAGE_VALUES]);
    other.reset();
    PGXP::LoadMemorySnapshot(anchor);
    CHECK(read_vertex(0x2000) == expected);
    PGXP::CPU_SW(2u << 16, packed, 0x4000);
    CHECK(read_vertex(0x4000) == expected);
    PGXP::CPU_MFC2((3u << 16) | (14u << 11), packed, packed);
    PGXP::CPU_SW(3u << 16, packed, 0x6000);
    CHECK(read_vertex(0x6000) == expected);
    if (cache)
      CHECK(read_vertex(0xffffffff) == expected_cache);
    PGXP::LoadMemorySnapshot(anchor);
    CHECK(read_vertex(0x2000) == expected);
    for (int site = 0; site < (cache ? 5 : 3); site++)
    {
      write_vertex(12.75f, 8.875f);
      auto preserved = anchor;
      allocation_hits = 0;
      fail_after = site;
      CHECK(!PGXP::SaveMemorySnapshot(&anchor));
      fail_after = -1;
      CHECK(allocation_hits == 1);
      CHECK(anchor == preserved);
      CHECK(read_vertex(0x2000) != expected);
      PGXP::LoadMemorySnapshot(anchor);
      CHECK(read_vertex(0x2000) == expected);
    }
    PGXP::TrimMemorySnapshots();
    PGXP::LoadMemorySnapshot(anchor);
    CHECK(read_vertex(0x2000) == expected);
    if (cache)
      CHECK(read_vertex(0xffffffff) == expected_cache);
    PGXP::Shutdown();
    CHECK(!PGXP::MemorySnapshotMatches(anchor));
  }
  std::printf("PASS %u actual PGXP consumer/ownership/failure checks\n",checks);
}
