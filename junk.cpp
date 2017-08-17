//
// Created by Andrew Cox in 2017.
//

#if 0

    // Stress test that background work is canceled when the transaction counter is changed:
    for(int i = 0; i < 100; ++i)
    {
        auto futureTiles = async_tiled::mandelbrotAsyncTiled(-2, 1, 1.5001f, -1.4999f, 32, transaction, transaction, tileGridDims, spec, tiles, framebuffer);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        ++transaction;
    }
    //std::this_thread::sleep_for(std::chrono::milliseconds(2));

void spawnConst(const SharedState& in, SharedState& out, SharedState inOut)
{
    //auto taskConst = [&in, &out] (int i, long double f) -> const SharedState &
    auto taskConst = [] (int i, long double f, const SharedState& in, SharedState& out) -> const SharedState &
    {
        out.counter1 = in.counter1;
        out.weighted1 = in.weighted1;
        for(int j = 0; j < i; ++j)
        {
            out.weighted1 += f;
        }
        out.counter1 += i;

        return in;
    };

    const int i = rand() / float(RAND_MAX) * 121;
    const float f = rand() / float(RAND_MAX);

    auto& direct = taskConst(i, f, in, out);

    auto stdFuture = std::async(std::launch::async, taskConst, i, f, in, out);
    //auto stlabFuture = stlab::async(stlab::default_executor, taskConst, i, f);

    //while(!stlabFuture.get_try()) {
    //    std::this_thread::yield();
    //}
}


    std::vector <stlab::future<const Tile2D &>>

    mandelbrotAsyncTiled1(
            const float left, const float right, const float top, const float bottom,
            const unsigned maxIters,
            const uint16_t originalTransaction,
            /// When this no longer matches originalTransaction, the async operations will be abandoned.
            const std::atomic<uint16_t>& transaction,
            const Dims2U tileGridDims, const TileSpec &spec, std::vector <Tile2D>& tiles, Framebuffer &framebuffer)
    {
        const Dims2U framebufferDims = pixelDims(spec, tileGridDims);

        TileSpec specCopy = spec;
        Tile2D tempTile {0,0};
        std::vector<stlab::future<const Tile2D &>> futureTiles;


        auto lambda = [&transaction, originalTransaction, top, left, bottom, right, framebufferDims, maxIters]
                (const TileSpec &spec, const Tile2D &tile, Framebuffer& framebuffer) -> const Tile2D &
        {
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
            // Use this to see a progressive load of tile:
            // std::this_thread::sleep_for(std::chrono::milliseconds(1*tile.x*tile.y));
            return tile;

        };

        std::async(std::launch::async, lambda, std::cref(specCopy), std::cref(tempTile), std::ref(framebuffer));

        //stlab::future<const Tile2D &> futureTile = stlab::async(stlab::default_executor,
        //                    //[top, left, bottom, right, maxIters, framebufferDims, originalTransaction, &transaction]
         //       lambda, specCopy, tempTile, framebuffer);//, transaction);
        //futureTiles.push_back(futureTile);
        return futureTiles;
    }

#endif // #if 0