var CACHE_NAME = 'gba-v1';

let scope = self.registration.scope
let prefix = scope.split('/', 4)[3] ? ('/' + scope.split('/', 4)[3]) : ''

var urlsToCache = [
    '/',
    '/localforage.js',
    '/a.out.js',
    '/a.out.wasm',
    '/icon.png'
];

for (var i = 0; i < urlsToCache.length; i++) {
    urlsToCache[i] = prefix + urlsToCache[i]
}

console.log(scope,prefix,urlsToCache)

self.addEventListener('install', function (event) {
    // Perform install steps
    event.waitUntil(
        caches.open(CACHE_NAME)
            .then(function (cache) {
                console.log('Opened cache');
                return cache.addAll(urlsToCache);
            })
    );
});


self.addEventListener('fetch', function (event) {
    event.respondWith(
        caches.match(event.request)
            .then(function (response) {
                // Cache hit - return response
                if (response) {
                    return response;
                }
                console.log('cache miss')
                return fetch(event.request);
            })
    );
});


self.addEventListener('activate', function (event) {
    console.log('activated, remove unused cache...')
    var cacheAllowlist = [CACHE_NAME];
    event.waitUntil(
        caches.keys().then(function (cacheNames) {
            return Promise.all(
                cacheNames.map(function (cacheName) {
                    if (cacheAllowlist.indexOf(cacheName) === -1) {
                        return caches.delete(cacheName);
                    }
                })
            );
        })
    );
});
