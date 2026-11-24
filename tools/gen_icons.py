#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Generates MANARA's flat icon set (PNG, 1x and @2x) from simple geometric
# primitives, so the whole set can be restyled by editing colours below.
#   python3 tools/gen_icons.py src/resources/images
import math, os, sys
from PIL import Image, ImageDraw

BLUE = (44, 107, 216, 255)      # #2C6BD8  stroke colour, readable on light and dark
ORANGE = (242, 138, 12, 255)    # #F28A0C
GREEN = (34, 160, 90, 255)
RED = (220, 60, 60, 255)
GREY = (140, 150, 172, 255)
WHITE = (255, 255, 255, 255)
SS = 8  # supersampling

class Canvas:
    """Draws on a 24x24 design grid, scaled to any pixel size."""
    def __init__(self, w, h, color=BLUE):
        self.w, self.h, self.c = w, h, color
        self.k = SS * min(w, h) / 24.0
        self.im = Image.new('RGBA', (w * SS, h * SS), (0, 0, 0, 0))
        self.d = ImageDraw.Draw(self.im)
        self.ox = (w * SS - 24 * self.k) / 2
        self.oy = (h * SS - 24 * self.k) / 2
        self.sw = 1.9  # stroke width in grid units
    def p(self, x, y): return (self.ox + x * self.k, self.oy + y * self.k)
    def line(self, pts, color=None, width=None):
        color = color or self.c; wpx = (width or self.sw) * self.k
        P = [self.p(*q) for q in pts]
        for a, b in zip(P, P[1:]):
            self.d.line([a, b], fill=color, width=int(round(wpx)))
        for q in (P[0], P[-1]) if len(P) else ():
            r = wpx / 2; self.d.ellipse([q[0]-r, q[1]-r, q[0]+r, q[1]+r], fill=color)
        for q in P[1:-1]:
            r = wpx / 2; self.d.ellipse([q[0]-r, q[1]-r, q[0]+r, q[1]+r], fill=color)
    def rrect(self, x0, y0, x1, y1, r, fill=None, outline=True, color=None):
        color = color or self.c
        a, b = self.p(x0, y0), self.p(x1, y1)
        if fill: self.d.rounded_rectangle([a, b], radius=r * self.k, fill=fill)
        if outline:
            self.d.rounded_rectangle([a, b], radius=r * self.k, outline=color, width=int(round(self.sw * self.k)))
    def circle(self, cx, cy, r, fill=None, outline=False, color=None, width=None):
        color = color or self.c
        a, b = self.p(cx - r, cy - r), self.p(cx + r, cy + r)
        self.d.ellipse([a, b], fill=fill, outline=color if outline else None,
                       width=int(round((width or self.sw) * self.k)) if outline else 0)
    def arc(self, cx, cy, r, a0, a1, color=None, width=None):
        color = color or self.c; wpx = int(round((width or self.sw) * self.k))
        a, b = self.p(cx - r, cy - r), self.p(cx + r, cy + r)
        self.d.arc([a, b], a0, a1, fill=color, width=wpx)
        for ang in (a0, a1):
            x = cx + r * math.cos(math.radians(ang)); y = cy + r * math.sin(math.radians(ang))
            q = self.p(x, y); rr = wpx / 2
            self.d.ellipse([q[0]-rr, q[1]-rr, q[0]+rr, q[1]+rr], fill=color)
    def poly(self, pts, fill):
        self.d.polygon([self.p(*q) for q in pts], fill=fill)
    def done(self):
        return self.im.resize((self.w, self.h), Image.LANCZOS)

# ---- glyphs (24x24 design grid) -------------------------------------------
def g_chat(c):
    c.rrect(3, 4, 21, 17, 4); c.line([(8, 17), (8, 21), (12, 17)])
def g_newchat(c):
    c.rrect(3, 4, 21, 17, 4); c.line([(12, 8), (12, 13)]); c.line([(9.5, 10.5), (14.5, 10.5)]); c.line([(8, 17), (8, 21), (12, 17)])
def g_chatroom(c):
    c.rrect(2.5, 3.5, 15.5, 13.5, 3.5); c.line([(6, 13.5), (6, 17), (9.5, 13.5)])
    c.rrect(9, 9, 21.5, 19, 3.5, color=ORANGE); c.line([(18, 19), (18, 22), (15, 19)], color=ORANGE)
def g_file(c):
    c.line([(6, 3), (14, 3), (19, 8), (19, 21), (6, 21), (6, 3)]); c.line([(14, 3), (14, 8), (19, 8)])
def g_folder(c):
    c.line([(3, 7), (3, 19), (21, 19), (21, 9), (11, 9), (9, 6), (3, 6), (3, 7)])
def g_sendfolder(c):
    g_folder(c); c.line([(12, 16), (12, 11.5)], color=ORANGE); c.line([(9.5, 13.5), (12, 11), (14.5, 13.5)], color=ORANGE)
def g_recvfolder(c):
    g_folder(c); c.line([(12, 11.5), (12, 16)], color=ORANGE); c.line([(9.5, 14), (12, 16.5), (14.5, 14)], color=ORANGE)
def g_broadcast(c):
    c.circle(12, 12, 2.6, fill=c.c)
    c.arc(12, 12, 6.5, -55, 55, width=2.2); c.arc(12, 12, 6.5, 125, 235, width=2.2)
    c.arc(12, 12, 10.5, -50, 50, width=2.2); c.arc(12, 12, 10.5, 130, 230, width=2.2)
def g_send(c):
    c.poly([(3, 11), (21, 3), (14, 21), (11.5, 13.5)], fill=c.c)
    c.line([(11.5, 13.5), (21, 3)], color=WHITE, width=1.3)
def g_smiley(c):
    c.circle(12, 12, 9, outline=True); c.circle(9, 10, 1.2, fill=c.c); c.circle(15, 10, 1.2, fill=c.c)
    c.arc(12, 12, 5.2, 25, 155)
def g_history(c):
    c.circle(12, 12, 9, outline=True); c.line([(12, 7), (12, 12), (15.5, 14)])
def g_refresh(c):
    c.arc(12, 12, 8, 30, 320); c.poly([(17, 2.5), (21, 8.5), (13.5, 8.5)], fill=c.c)
def g_close(c):
    c.line([(6, 6), (18, 18)]); c.line([(18, 6), (6, 18)])
def g_accept(c):
    c.line([(4.5, 12.5), (9.5, 17.5), (19.5, 6.5)], color=GREEN, width=2.3)
def g_stop(c):
    c.rrect(6, 6, 18, 18, 2.5, fill=c.c, outline=False)
def g_play(c):
    c.poly([(8, 4.5), (19, 12), (8, 19.5)], fill=c.c)
def g_info(c):
    c.circle(12, 12, 9, outline=True); c.line([(12, 11), (12, 17)]); c.circle(12, 7.6, 1.2, fill=c.c)
def g_question(c):
    c.circle(12, 12, 9, outline=True); c.arc(12, 10, 3, 180, 380); c.line([(12, 13), (12, 14.5)]); c.circle(12, 17.2, 1.1, fill=c.c)
def g_critical(c):
    c.poly([(12, 3), (22, 20), (2, 20)], fill=RED); c.line([(12, 9), (12, 14)], color=WHITE, width=2); c.circle(12, 17, 1.1, fill=WHITE)
def g_home(c):
    c.line([(3, 12), (12, 4), (21, 12)]); c.line([(6, 10.5), (6, 20), (18, 20), (18, 10.5)])
def g_save(c):
    c.rrect(4, 4, 20, 20, 2.5); c.rrect(8, 4, 16, 9, 1, outline=True); c.rrect(7.5, 13, 16.5, 20, 1, outline=True)
def g_tools(c):
    c.circle(8.5, 8.5, 4.5, outline=True); c.line([(11.5, 11.5), (20, 20)], width=2.6)
def g_paint(c):
    c.circle(12, 12, 9, outline=True)
    for x, y, col in ((8, 9, ORANGE), (13, 7.5, GREEN), (16.5, 11.5, RED), (8.5, 14.5, BLUE)): c.circle(x, y, 1.3, fill=col)
def g_font(c):
    c.line([(5, 19), (12, 4), (19, 19)]); c.line([(8, 14), (16, 14)])
def g_fontcolor(c):
    g_font(c); c.line([(4, 22), (20, 22)], color=ORANGE, width=2.2)
def g_color(c):
    c.circle(12, 12, 9, fill=ORANGE, outline=False)
def g_web(c):
    c.circle(12, 12, 9, outline=True); c.line([(3, 12), (21, 12)], width=1.5)
    c.d.ellipse([c.p(7.5, 3), c.p(16.5, 21)], outline=c.c, width=int(1.5 * c.k))
def g_addcontact(c):
    c.circle(10, 8.5, 3.8, outline=True); c.arc(10, 21, 7.5, 200, 340)
    c.line([(19, 7), (19, 13)], color=ORANGE); c.line([(16, 10), (22, 10)], color=ORANGE)
def g_messenger(c):
    c.rrect(3, 4, 21, 17, 4); c.line([(8, 17), (8, 21), (12, 17)])
def g_transfer(c):
    c.line([(5, 8), (19, 8)]); c.line([(15.5, 4.5), (19, 8), (15.5, 11.5)])
    c.line([(19, 16), (5, 16)], color=ORANGE); c.line([(8.5, 12.5), (5, 16), (8.5, 19.5)], color=ORANGE)
def g_gear(c):
    for i in range(8):
        a = math.radians(i * 45); x0, y0 = 12 + 6.2 * math.cos(a), 12 + 6.2 * math.sin(a)
        x1, y1 = 12 + 9.2 * math.cos(a), 12 + 9.2 * math.sin(a); c.line([(x0, y0), (x1, y1)], width=3.0)
    c.circle(12, 12, 6.4, fill=c.c); c.circle(12, 12, 2.6, fill=(0, 0, 0, 0))
    c.d.ellipse([c.p(9.4, 9.4), c.p(14.6, 14.6)], fill=(0, 0, 0, 0))
def g_user(c):
    c.circle(12, 8, 4, outline=True); c.arc(12, 22, 9, 205, 335)
def g_net(c):
    c.arc(12, 19, 5, 215, 325); c.arc(12, 19, 9.5, 220, 320); c.arc(12, 19, 14, 225, 315); c.circle(12, 18.5, 1.3, fill=c.c)
def g_bell(c):
    c.line([(6, 17), (6, 11), (7, 8), (12, 5), (17, 8), (18, 11), (18, 17), (20, 18.5), (4, 18.5), (6, 17)]); c.arc(12, 19.5, 2.2, 10, 170)
def g_kbd(c):
    c.rrect(2.5, 6.5, 21.5, 17.5, 2.5)
    for x in (6.5, 10, 13.5, 17): c.circle(x, 10.5, 0.9, fill=c.c)
    c.line([(7, 14), (17, 14)], width=1.6)

# status dots: filled disc with a lighter ring (reads on any background)
def status(color, glyph=None):
    def f(c):
        c.circle(12, 12, 8.6, fill=color)
        if glyph == 'bar': c.line([(7.5, 12), (16.5, 12)], color=WHITE, width=2.4)
        if glyph == 'clock': c.line([(12, 7), (12, 12), (15.5, 13.5)], color=WHITE, width=2.0)
        if glyph == 'ring': c.circle(12, 12, 4.4, fill=(0, 0, 0, 0)); 
    return f
def offline(c):
    c.circle(12, 12, 7.6, outline=True, color=GREY, width=2.2)

ICONS = {
    # 16px toolbar/menu icons
    'chat.png': (24, g_chat), 'newchatroom.png': (24, g_newchat), 'chatroom.png': (16, g_chatroom),
    'file.png': (16, g_file), 'filemsg.png': (16, g_file), 'folder.png': (16, g_folder),
    'sendfolder.png': (16, g_sendfolder), 'receivefolder.png': (16, g_recvfolder),
    'broadcastmsg.png': (16, g_broadcast), 'send.png': (16, g_send), 'smiley.png': (16, g_smiley),
    'history.png': (16, g_history), 'refresh.png': (16, g_refresh), 'close.png': (16, g_close),
    'accept.png': (16, g_accept), 'stop.png': (16, g_stop), 'play.png': (16, g_play), 'info.png': (16, g_info),
    'question.png': (16, g_question), 'critical.png': (16, g_critical), 'criticalmsg.png': (16, g_critical),
    'home.png': (16, g_home), 'save.png': (16, g_save), 'tools.png': (16, g_tools), 'paint.png': (16, g_paint),
    'font.png': (16, g_font), 'fontcolor.png': (16, g_fontcolor), 'color.png': (16, g_color), 'web.png': (16, g_web),
    'messenger.png': (16, g_messenger), 'transfer.png': (16, g_transfer),
    'addcontact.png': (24, g_addcontact),
    # 32px settings page icons
    'systemsettings.png': (32, g_gear), 'usersettings.png': (32, g_user), 'networksettings.png': (32, g_net),
    'alertsettings.png': (32, g_bell), 'historysettings.png': (32, g_history), 'messagesettings.png': (32, g_chat),
    'hotkeysettings.png': (32, g_kbd), 'themesettings.png': (32, g_paint), 'transfersettings.png': (32, g_transfer),
    # 20px status icons and tray bubbles
    'online.png': (20, status(GREEN)), 'away.png': (20, status(ORANGE, 'clock')), 'busy.png': (20, status(RED)),
    'nodisturb.png': (20, status(RED, 'bar')), 'offline.png': (20, offline),
    'onlinebubble.png': (20, status(GREEN)), 'awaybubble.png': (20, status(ORANGE, 'clock')),
    'busybubble.png': (20, status(RED)), 'nodisturbbubble.png': (20, status(RED, 'bar')), 'offlinebubble.png': (20, offline),
}

def main(out):
    for name, (size, fn) in ICONS.items():
        for scale, suffix in ((1, ''), (2, '@2x')):
            c = Canvas(size * scale, size * scale); fn(c)
            c.done().save(os.path.join(out, name.replace('.png', suffix + '.png')))
    print(len(ICONS), 'icons x2 written to', out)

if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else '.')
