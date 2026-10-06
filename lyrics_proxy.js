const http = require('http');
const https = require('https');
const url = require('url');

const PORT = process.env.PORT || 8080;
const cache = new Map();

function fetchHttps(urlStr) {
    return new Promise((resolve, reject) => {
        const req = https.get(urlStr, {
            headers: {
                'User-Agent': 'ESP32-Lyrics-Display/1.0',
                'Accept': 'application/json'
            }
        }, (res) => {
            let data = '';
            res.on('data', chunk => data += chunk);
            res.on('end', () => {
                if (res.statusCode >= 200 && res.statusCode < 300) {
                    try {
                        resolve(JSON.parse(data));
                    } catch (e) {
                        resolve(null);
                    }
                } else {
                    resolve(null);
                }
            });
        });
        req.on('error', (err) => resolve(null));
        req.setTimeout(5000, () => {
            req.destroy();
            resolve(null);
        });
    });
}

async function getSyncedLyrics(artist, title, duration) {
    const cacheKey = `${artist.toLowerCase()}|${title.toLowerCase()}`;
    if (cache.has(cacheKey)) {
        console.log(`[PROXY] Cache hit for: "${title}" by "${artist}"`);
        return cache.get(cacheKey);
    }

    console.log(`[PROXY] Fetching LRCLIB for: "${title}" by "${artist}" (dur: ${duration})`);

    // 1. Try exact match with duration
    if (artist && title && duration > 0) {
        const getUrl = `https://lrclib.net/api/get?artist_name=${encodeURIComponent(artist)}&track_name=${encodeURIComponent(title)}&duration=${duration}`;
        const data = await fetchHttps(getUrl);
        if (data && data.syncedLyrics) {
            cache.set(cacheKey, data.syncedLyrics);
            return data.syncedLyrics;
        }
    }

    // 2. Try exact match without duration
    if (artist && title) {
        const getUrl = `https://lrclib.net/api/get?artist_name=${encodeURIComponent(artist)}&track_name=${encodeURIComponent(title)}`;
        const data = await fetchHttps(getUrl);
        if (data && data.syncedLyrics) {
            cache.set(cacheKey, data.syncedLyrics);
            return data.syncedLyrics;
        }
    }

    // 3. Search query: artist + title
    const query = artist ? `${artist} ${title}` : title;
    const searchUrl = `https://lrclib.net/api/search?q=${encodeURIComponent(query)}`;
    const results = await fetchHttps(searchUrl);
    if (Array.isArray(results)) {
        for (const item of results) {
            if (item && item.syncedLyrics) {
                cache.set(cacheKey, item.syncedLyrics);
                return item.syncedLyrics;
            }
        }
    }

    // 4. Search query: title only
    if (title) {
        const titleSearchUrl = `https://lrclib.net/api/search?q=${encodeURIComponent(title)}`;
        const titleResults = await fetchHttps(titleSearchUrl);
        if (Array.isArray(titleResults)) {
            for (const item of titleResults) {
                if (item && item.syncedLyrics) {
                    cache.set(cacheKey, item.syncedLyrics);
                    return item.syncedLyrics;
                }
            }
        }
    }

    cache.set(cacheKey, null);
    return null;
}

const server = http.createServer(async (req, res) => {
    const parsed = url.parse(req.url, true);
    console.log(`[PROXY] Request: ${req.url}`);

    let artist = parsed.query.artist_name || parsed.query.artist || '';
    let title = parsed.query.track_name || parsed.query.track || parsed.query.title || '';
    let duration = parseInt(parsed.query.duration || '0', 10);

    // Also support /api/search?q=
    if (!title && parsed.query.q) {
        title = parsed.query.q;
    }

    if (!title && !artist) {
        res.writeHead(400, { 'Content-Type': 'text/plain', 'Connection': 'close' });
        res.end('Missing artist or title');
        return;
    }

    try {
        const lrc = await getSyncedLyrics(artist, title, duration);

        if (lrc && lrc.trim().length > 0) {
            const buf = Buffer.from(lrc, 'utf-8');
            res.writeHead(200, {
                'Content-Type': 'text/plain; charset=utf-8',
                'Content-Length': buf.length,
                'Connection': 'close'
            });
            res.end(buf);
            console.log(`[PROXY] Sent ${buf.length} bytes of synced lyrics`);
        } else {
            const msg = 'No synced lyrics found';
            res.writeHead(404, {
                'Content-Type': 'text/plain',
                'Content-Length': Buffer.byteLength(msg),
                'Connection': 'close'
            });
            res.end(msg);
            console.log(`[PROXY] No lyrics found (404)`);
        }
    } catch (err) {
        console.error(`[PROXY] Handler error: ${err.message}`);
        res.writeHead(500, { 'Content-Type': 'text/plain', 'Connection': 'close' });
        res.end('Internal Server Error');
    }
});

server.listen(PORT, '0.0.0.0', () => {
    console.log(`[PROXY] Fast Lyrics Server running on port ${PORT}`);
});

