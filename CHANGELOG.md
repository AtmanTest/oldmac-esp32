# Journal des versions — oldmac-esp32

## 1.1.0 — 2026-11-22 — SAFARI, le web en texte

### Ajoute
- Icone et fenetre **Safari** sur le bureau (7e icone) : la carte tire une
  page par HTTPS et l'affiche en texte dans une fenetre du Macintosh.
- `main/safari_txt.cpp` : convertisseur HTML -> texte en flux, sans ESP-IDF
  (scripts, styles, commentaires et balises jetes ; entites decodees ; accents
  plies ; lignes coupees a 37 colonnes, jamais au milieu d'un mot).
- `main/safari.cpp` : tache de chargement (pile en PSRAM), TLS verifie
  (bundle IDF), corps lu a la main, echec affiche (code HTTP ou RESEAU).
- Chargement automatique de `https://www.oodrive.com/` des que le reseau
  est la, et a l'ouverture de l'icone ; B1 recharge, B2 tourne les pages.
- API : `/safari` (recharger, `?url=` optionnel), bloc `safari` dans `/etat`,
  `/vue?n=7`.
- `tests/run_unit.sh` + `tests/unit/test_safari.cpp` : 33 verifications sur
  le convertisseur (balises, scripts, entites, accents, pliage, flux par
  morceaux, saturation a 240 lignes).
- `tools/apercu.cpp` accepte `safari`, `safari-attente`, `safari-echec` ;
  `tools/safari_stub.cpp` permet de rendre la fenetre sur le PC.

### Corrige
- L'etat du wifi est lu sur l'interface (`esp_netif_get_ip_info`) et non plus
  sur un drapeau : l'evenement IP_EVENT n'arrivait pas, l'icone affichait
  « hors ligne » alors que le reseau etait monte et que l'API repondait.
- `secrets.h` pointe sur le reseau reellement utilise (GL-BE10000-baa).

### Mesure
- Image : 0x10d6c0 octets, 27 % libres dans la partition app de 1,5 Mo.
- Page oodrive.com : HTTP 200, 617 472 octets lus, 240 lignes, 19 pages,
  4 110 ms, TLS verifie.

## 1.0.0 — 2026-11-20 — premier oldmac-esp32

- Macintosh miniature sur la dalle 200x200 en 1 bit : disquette, Happy Mac,
  bureau, 6 icones, fenetres MacWrite / MacPaint / Tableau de bord / A propos /
  Horloge, deux boutons, pied qui rappelle les gestes.
- Mesures reelles (memoire, PSRAM, temperature, uptime), heure par SNTP,
  wifi a trois reseaux, API `/etat` `/ecran` `/vue` `/bouton`.
