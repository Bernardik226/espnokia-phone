// service worker do painel do Claw'd: cacheia a casca (HTML + ícones) pra abrir
// instantâneo e instalar como app; os dados (API) sempre vão pela rede.
const CACHE = "espnokia-claud-v5";
const SHELL = ["/", "/static/app.css", "/static/app.js",
               "/static/icon-192.png", "/static/icon-512.png"];

self.addEventListener("install", (e) => {
  e.waitUntil(caches.open(CACHE).then((c) => c.addAll(SHELL)));
  self.skipWaiting();
});

self.addEventListener("activate", (e) => {
  e.waitUntil(
    caches.keys().then((ks) =>
      Promise.all(ks.filter((k) => k !== CACHE).map((k) => caches.delete(k))))
  );
  self.clients.claim();
});

self.addEventListener("fetch", (e) => {
  if (e.request.method !== "GET") return;
  const p = new URL(e.request.url).pathname;
  // dados sempre frescos: nunca cacheia a API do device/Claw'd
  if (/^\/(admin|claude|copa|futebol|health)/.test(p)) return;
  // casca: REDE primeiro (sempre pega a versão nova no deploy), cache só como
  // reserva offline — evita o SW servir app.js/css velho e quebrar a tela
  e.respondWith(
    caches.match(e.request).then((hit) => {
      return fetch(e.request).then((res) => {
        if (res && res.status === 200)
          caches.open(CACHE).then((c) => c.put(e.request, res.clone()));
        return res;
      }).catch(() => hit);
    })
  );
});
