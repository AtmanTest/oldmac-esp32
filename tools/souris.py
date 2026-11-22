#!/usr/bin/env python3
"""souris.py — la souris du Macintosh miniature, depuis le PC.

La carte ne peut pas recevoir de souris USB (son port sert au flash et au
5 V d'entree) : c'est donc le PC qui tient le pointeur. Cette fenetre
affiche l'ecran de la carte (100x100 en texte, lu sur /ecran) et envoie
chaque clic en pixels a /clic?x=&y=.

    tools/souris.py 192.168.8.209          (ou --hote 192.168.8.209)
    tools/souris.py                       (demande l'adresse)

Touches : B1/B2 comme sur la carte (appui court / double / long).
"""
import sys
import json
import threading
import tkinter as tk
import urllib.request
import urllib.error

ZOOM = 3                      # 100x100 -> 300x300 lus, agrandis par ZOOM
PERIODE_MS = 900              # la dalle met ~1,7 s a se rafraichir


class Souris:
    def __init__(self, hote):
        self.hote = hote
        self.racine = tk.Tk()
        self.racine.title("oldmac — souris")
        self.racine.configure(bg="#1a1a1a")
        self.racine.resizable(False, False)

        self.canvas = tk.Canvas(self.racine, width=100 * ZOOM, height=100 * ZOOM,
                                bg="white", highlightthickness=0, cursor="crosshair")
        self.canvas.grid(row=0, column=0, columnspan=4, padx=8, pady=(8, 4))
        self.canvas.bind("<Button-1>", self.clic)

        barre = tk.Frame(self.racine, bg="#1a1a1a")
        barre.grid(row=1, column=0, columnspan=4, sticky="we", padx=8, pady=(0, 8))
        for i, (txt, b, t) in enumerate((("B1 court", 0, "court"), ("B1 double", 0, "double"),
                                         ("B2 court", 1, "court"), ("B2 double", 1, "double"),
                                         ("B2 long", 1, "long"))):
            tk.Button(barre, text=txt, relief="flat", bg="#2d2d2d", fg="#ddd",
                      activebackground="#3d3d3d", activeforeground="#fff",
                      command=lambda b=b, t=t: self.bouton(b, t)).grid(row=0, column=i, padx=2)

        self.etat = tk.Label(self.racine, text="connexion...", bg="#1a1a1a", fg="#888",
                             font=("monospace", 9), anchor="w")
        self.etat.grid(row=2, column=0, columnspan=4, sticky="we", padx=8, pady=(0, 8))

        self.racine.bind("<Key-1>", lambda e: self.bouton(0, "court"))
        self.racine.bind("<Key-2>", lambda e: self.bouton(1, "court"))
        self.racine.bind("<Key-3>", lambda e: self.bouton(1, "double"))

        self.image = None
        self.racine.after(200, self.rafraichit)

    # ---------------------------------------------------------------- HTTP
    def get(self, chemin, timeout=4):
        with urllib.request.urlopen(f"http://{self.hote}{chemin}", timeout=timeout) as r:
            return r.read().decode("utf-8", "replace")

    def clic(self, ev):
        x = min(199, ev.x // ZOOM * 2)
        y = min(199, ev.y // ZOOM * 2)
        try:
            self.get(f"/clic?x={x}&y={y}")
            self.etat.config(text=f"clic {x},{y}")
        except Exception as e:
            self.etat.config(text=f"clic impossible : {e}")
        self.racine.after(700, self.rafraichit)

    def bouton(self, b, t):
        try:
            self.get(f"/bouton?b={b}&t={t}")
            self.etat.config(text=f"bouton {b} {t}")
        except Exception as e:
            self.etat.config(text=f"bouton impossible : {e}")
        self.racine.after(700, self.rafraichit)

    # ---------------------------------------------------------------- ecran
    def rafraichit(self):
        try:
            txt = self.get("/ecran", timeout=6)
            lignes = [l for l in txt.splitlines() if l.strip()]
            self.dessine(lignes)
            info = json.loads(self.get("/etat"))
            self.etat.config(text="vue %d  wifi %s  %s  %s" % (
                info.get("vue", -1), "ok" if info.get("wifi") else "off",
                info.get("heure", "--:--"), info.get("ip", "n.d.")))
        except Exception as e:
            self.etat.config(text=f"carte injoignable ({self.hote}) : {e}")
        self.racine.after(PERIODE_MS, self.rafraichit)

    def dessine(self, lignes):
        self.canvas.delete("all")
        for j, ligne in enumerate(lignes[:100]):
            for i, c in enumerate(ligne[:100]):
                if c == "#":
                    couleur = "black"
                elif c == "+":
                    couleur = "#777"
                else:
                    continue
                x0, y0 = i * ZOOM, j * ZOOM
                self.canvas.create_rectangle(x0, y0, x0 + ZOOM, y0 + ZOOM,
                                             fill=couleur, width=0)

    def tourne(self):
        self.racine.mainloop()


if __name__ == "__main__":
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    hote = args[0] if args else None
    if not hote:
        try:
            import tkinter.simpledialog as sd
            r = tk.Tk(); r.withdraw()
            hote = sd.askstring("oldmac — souris", "adresse de la carte :", initialvalue="192.168.8.209")
            r.destroy()
        except Exception:
            pass
    if not hote:
        print("usage : tools/souris.py <ip>", file=sys.stderr)
        sys.exit(2)
    Souris(hote).tourne()
