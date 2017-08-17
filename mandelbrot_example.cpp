//
// Copyright Andrew Cox 2017. All rights reserved.
//

#include <atomic>
#include <iostream>
#include <thread>
#include <complex>
#include <functional>
#include <future>

#include "stlab/concurrency/future.hpp"
#include "stlab/concurrency/default_executor.hpp"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

// You might want to edit this for your platform:
constexpr const char * const OUTPUT_PATH_MANDELBROT = "/tmp/stlab-mandelbrot.png";

namespace async_tiled {
    using namespace stlab;

    enum class TileFormat {
        RGBA8888 = 1
    };

    /** A byte-per-component pixel. */
    struct RGBA {
        RGBA() {}

        RGBA(unsigned r, unsigned g, unsigned b, unsigned a) : r(r), g(g), b(b), a(a) {}

        static constexpr TileFormat format = TileFormat::RGBA8888;
        uint8_t r;
        uint8_t g;
        uint8_t b;
        uint8_t a;

        constexpr bool operator==(const RGBA &rhs) const { return r == rhs.r && g == rhs.g && b == rhs.b && a == rhs.a; }
    };

    using Framebuffer = std::vector<RGBA>;

    struct Dims2U {
        unsigned w;
        unsigned h;
    };

    struct Point2U {
        unsigned x;
        unsigned y;
    };

    /**
     * A bundle of data related to a related group of tiles (like all the tiles in a
     * framebuffer).
     */
    struct TileSpec {
        TileSpec(const TileFormat pixelFormat, const uint16_t w, const uint16_t h, const unsigned stride) :
                pixelFormat(pixelFormat), w(w), h(h), stride(stride) {}
        /** Currently only one format is in use. */
        /// @note We could avoid specifying the format here and let the functions
        /// processing tiles define the data stored in them implicitly.
        const TileFormat pixelFormat = TileFormat::RGBA8888;
        /** Width of tile. */
        const uint16_t w;
        /** Height of tile. */
        const uint16_t h;
        /** The distance in bytes between scanlines of the tile in a pixel buffer. */
        const unsigned stride;
    };

    /**
     * A bundle of pixel data. Derived classes know the format of the pixels and the
     * ownership of them.
     */
    struct Tile2D {
        Tile2D(uint16_t x, uint16_t y) : x(x), y(y) {
            pixels = nullptr;
        }

        Tile2D(uint8_t *pixels, uint16_t x, uint16_t y) : pixels(pixels), x(x), y(y) {
        }

        virtual ~Tile2D() {}

        /** Pointer to pixels that are not necessarily owned. */
        uint8_t *pixels;
        /** Logical x position of tile in image. */
        uint16_t x;
        /** Logical y coordinate of tile in image. */
        uint16_t y;
    };

    /**
     * Work out how big a framebuffer is that uses all tiles in a grid of them.
     * @param spec
     * @param tileGridDims
     * @return framebuffer width and height.
     */
    constexpr Dims2U pixelDims(const TileSpec& spec, const Dims2U tileGridDims)
    {
        return  {spec.w * tileGridDims.w, spec.h * tileGridDims.h};
    }

    /**
     * Work out the address of the leftmost pixel of the given row of a tile.
     * @param spec
     * @param tile
     * @param y coordinate of row within tile.
     * @return Pointer to the first pixel of the row.
     */
    template<typename PixelType>
    constexpr PixelType* addressRow(const TileSpec& spec, const Tile2D& tile, const unsigned y) {
        PixelType *pixelRow = reinterpret_cast<PixelType *>(tile.pixels + spec.stride * y);
        return pixelRow;
    }

    /**
     * The position of a tile in the overall framebuffer.
     * @param spec
     * @param tile
     * @return Coordinates in framebuffer pixels of the tile upper-left.
     */
    constexpr Point2U pixelPosition(const TileSpec& spec, const Tile2D& tile)
    {
        Point2U position = {unsigned(spec.w) * tile.x, unsigned(spec.h) * tile.y};
        return position;
    }

    /**
     * Launch a function to run asynchronously on each tile of a framebuffer,
     * where the tiles point into a common framebuffer.
     * @return A vector of futures of whatever the launched function returns,
     * which by convention should be references to tiles in outTiles.
     */
    template<typename Executor, typename PixelType, typename Fn, typename... Args>
    std::vector<stlab::future<typename std::result_of<Fn(const TileSpec& spec, Tile2D& tile, Args&&...)>::type>>
    LaunchTiles(Executor& ex, const TileSpec &spec, const Dims2U bufferTiles,
                std::vector<PixelType> &framebuffer,
                std::vector<Tile2D> &outTiles,
                Fn &&func, Args &&... args)
    {
        outTiles.clear();
        outTiles.reserve(bufferTiles.w * bufferTiles.h);
        ///@ToDo - Pass this in to be reused.
        std::vector<stlab::future<typename std::result_of<Fn(const TileSpec& spec, Tile2D& tile, Args...)>::type>> tasks;
        tasks.reserve(outTiles.size());
        for(unsigned y = 0; y < bufferTiles.h; ++y)
        {
            for(unsigned x = 0; x < bufferTiles.w; ++x)
            {
                uint8_t * const tile_corner = reinterpret_cast<uint8_t*>(&framebuffer[0]) + y * spec.h * spec.stride + x * spec.w * sizeof(PixelType);
                outTiles.emplace(outTiles.end(), tile_corner, uint16_t(x), uint16_t(y));
                auto task = stlab::async(ex, std::forward<Fn>(func), spec, std::ref(outTiles.back()), std::forward<Args>(args)...);
                tasks.push_back(std::move(task));
            }
        }
        return tasks;
    }

    // Define the code to run on each tile:
    auto tileMandelbrotLambda = [ ]
           (const TileSpec &spec,
            Tile2D &tile,
            const float top, const float left, const float bottom, const float right,
            const unsigned maxIters,
            const Dims2U framebufferDims,
            const uint16_t originalTransaction,
            std::atomic<uint16_t>& transaction
           ) -> Tile2D *
    {
       //std::cerr << std::endl << "Tile " << tile.x << ", " << tile.y;
        std::cerr << (std::string("\nTile ") + std::to_string(tile.x) + ", " + std::to_string(tile.y));
        const Point2U framebufferPosition = pixelPosition(spec, tile);
        for (unsigned y = 0; y < spec.h; ++y) {
            // Allow cancelation per scanline so we don't burn cycles if this tile becomes
            // out of date before it is even fully generated:
            if(transaction != originalTransaction)
            {
                break;
            }
            const unsigned framebufferY = framebufferPosition.y + y;
            const float j = top + (bottom - top) / framebufferDims.h * framebufferY;
            RGBA *const pixelRow = addressRow<RGBA>(spec, tile, y);
            for (unsigned x = 0; x < spec.w; ++x) {
                const unsigned frameBufferX = framebufferPosition.x + x;
                const float i = left + (right - left) / framebufferDims.w * frameBufferX;
                const std::complex<float> c = {i, j};
                std::complex<float> z = {0, 0};
                unsigned iter = 0;
                for (; iter < maxIters; ++iter) {
                    z = z * z + c;
                    if (fabsf(z.real() * z.imag()) >= 4.0f) {
                        break;
                    }
                }
                const uint8_t grey = uint8_t(255.0f / maxIters * (maxIters - iter));
                pixelRow[x] = {grey, grey, grey, 255};
            }
        }
        // Use this to delay tiles by a screen position dependent amount and so see them load progressively:
        // std::this_thread::sleep_for(std::chrono::milliseconds(1*tile.x*tile.y));
        return &tile;
    };

    /**
     * Draw a mandelbrot set, making each tile of the image its own async task.
     **/
    std::vector <stlab::future<Tile2D *>> mandelbrotAsyncTiled(
            const float left, const float right, const float top, const float bottom,
            const unsigned maxIters,
            const uint16_t originalTransaction,
            /// When this no longer matches originalTransaction, the async operations will be abandoned.
            std::atomic<uint16_t>& transaction,
            const Dims2U tileGridDims, const TileSpec &spec, std::vector <Tile2D>& tiles, Framebuffer &framebuffer)
    {
        const Dims2U framebufferDims = pixelDims(spec, tileGridDims);
        
        std::vector <stlab::future<Tile2D *>> futureTiles =
                LaunchTiles(default_executor, spec, tileGridDims, framebuffer, tiles, tileMandelbrotLambda, top, left, bottom, right, maxIters, framebufferDims, originalTransaction, std::ref(transaction));

        return futureTiles;
    }

} // async_tiled

int main()
{
    constexpr unsigned tileDim = 32;
    constexpr async_tiled::Dims2U framebufferDims { 2048, 1280 };
    const async_tiled::Dims2U tileGridDims {framebufferDims.w / tileDim, framebufferDims.h / tileDim};
    const async_tiled::TileSpec spec {
            async_tiled::TileFormat::RGBA8888,
            tileDim, tileDim,
            framebufferDims.w * sizeof(async_tiled::RGBA)
    };
    std::vector <async_tiled::Tile2D> tiles;
    async_tiled::Framebuffer framebuffer(framebufferDims.w * framebufferDims.h);
    std::atomic<uint16_t> transaction(0);

    // Spawn background tasks to compute the Mandelbrot set over rectangular tiles of the framebuffer:
    auto futureTiles = async_tiled::mandelbrotAsyncTiled(-2, 1, 1.5001f, -1.4999f, 32, transaction, transaction, tileGridDims, spec, tiles, framebuffer);

    // Use stlab::wait_all() to set a variable when all tasks have completed:
#if 0
    using WaitContainer = std::vector <async_tiled::Tile2D *>;
    std::atomic<uint32_t> done { 0 };
    auto doneTrigger = [&done](const WaitContainer& futures){
        std::cerr << "\nDone " << futures.size() << " tiles." << std::endl;
        done = 1;
    };

    // Note, you must capture the future returned from `when_all()` into a variable
    // otherwise your completion functor will not fire.
    auto range = std::make_pair(futureTiles.begin(), futureTiles.end());
    auto futureAll = stlab::when_all(stlab::default_executor, doneTrigger, range);
    assert(futureAll.valid() == true);
    assert(futureAll.error().is_initialized() == false);
    std::cerr << std::endl;

    // Yield this processor to background tasks until they are all completed:
    while(!done) {
        std::this_thread::yield();
        //std::this_thread::sleep_for(std::chrono::milliseconds(7));
        std::cerr << " <*>";
    };
#endif

    // An alternative wait for all futures to be ready, more suitable to this usage:
    unsigned complete = 0;
    for(auto& future : futureTiles)
    {
        test_future:
        assert(future.valid());
        auto res = future.get_try();
        if(res){
            ++complete;
        } else {
            std::cerr << " <*>";
            std::this_thread::yield();
            goto test_future;
        }
    }

    std::cerr << std::endl << "Num complete = " << complete << " of " << futureTiles.size() << std::endl;

    std::cerr << "Saving image as PNG at \"" << OUTPUT_PATH_MANDELBROT << "\" ... ";
    auto pngResult = stbi_write_png(OUTPUT_PATH_MANDELBROT, framebufferDims.w, framebufferDims.h, 4, &framebuffer[0], framebufferDims.w * sizeof(async_tiled::RGBA));
    std::cerr << "PNG write result: " << pngResult << std::endl;

    std::cerr << std::endl << "Exiting." << std::endl;
    return 0;
}
