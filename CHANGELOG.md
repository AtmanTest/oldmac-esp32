# Journal des versions — oldmac-esp32

## 1.2.2 — 2026-11-22 — la version ne peut plus diverger

### Corrige
- `OM_VERSION` est maintenant fourni par la compilation, lu dans `VERSION`
  (`main/CMakeLists.txt`), et `VERSION` est declaree dependance de
  configuration : changer le fichier relance cmake, la compilation et la
  chaine annoncee. Prouve en passant `VERSION` a 9.9.9 : l'image contenait
  9.9.9, puis 1.2.2 apres restauration. La version de la 1.2.1 etait encore
  ecrite en dur dans `app.h`.

## 1.2.1 — 2026-11-22 — la version annoncée suit VERSION

### Corrige
- `OM_VERSION` était resté à « 1.0.0 » : `/etat` et la fenêtre « A propos »
  annonçaient 1.0.0 alors que VERSION disait 1.2.0. Les deux disent 1.2.0.

## 1.2.0 — 2026-11-22 — une souris pour le Macintosh

Pas de souris USB possible sur cette carte (le port sert au flash et à
l'alimentation : en mode hôte on perd la console, et rien ne garantit le 5 V
sur VBUS). Le pointeur vient donc du PC, par le réseau.

### Ajoute
- `GET /clic?x=&y=` : un clic en pixels (0..199), traité comme un doigt placé.
  Bureau : ouvre l'icône touchée (corbeille reconnue) ; fenêtre : la barre de
  titre ou le pied ferment, le corps tourne la page ; SAFARI se recharge en
  s'ouvrant.
- Pointeur dessiné sur la dalle (flèche 7x11, liseré blanc pour rester lisible
  sur le texte), position exposée dans `/etat` → `"souris":[x,y]`.
- `tools/souris.py` : fenêtre du PC qui affiche l'écran de la carte (via
  `/ecran`) et envoie chaque clic ; boutons B1/B2 et touches 1/2/3 ;
  entrée de menu « SOURIS » (oldmac-souris.desktop).
- `tests/unit/test_souris.cpp` : 55 vérifications de géométrie — chaque icône
  se trouve à son centre, Safari en 7e position, corbeille à 99, vide à -1,
  aucune icône hors dalle ni qui se recouvre.
- `tools/apercu.cpp clic` : rendu du pointeur sur le bureau, vérifié à l'œil
  avant tout flash.

### Etat de vérification
- Compilé (image 0x10dcd0, 27 % libres) et testé sur le PC : 88 vérifications.
- PAS encore vérifié sur la dalle : la carte s'est déconnectée de l'USB avant
  le flash. À faire au prochain branchement (un `idf.py flash` + un clic).

## 1.1.1 — 2026-11-22 — l'heure et le reseau disent la verite

### Corrige
- `heure_ok` n'etait jamais mis a vrai : l'horloge restait a « --:-- » et la
  fenetre Horloge affichait « heure non synchronisee » alors que le reseau
  etait monte. Il est maintenant deduit de l'horloge (SNTP a repondu quand
  `time()` depasse 2023).
- SNTP demarre depuis la lecture de l'interface reseau (l'evenement IP_EVENT
  n'arrive pas sur cette carte, il ne pouvait donc rien declencher).
- Fuseau horaire : `TZ=CET-1CEST` pose au demarrage ; sans lui l'heure SNTP
  s'affichait en UTC (09:26 a 10:26 a Paris).

### Mesure
- `/etat` sur la carte : `wifi:true`, `ip:192.168.8.209`, `heure:10:29`,
  `heure_ok:true`, `safari:{etat:2, code:200, lignes:240, pages:19}`.

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
