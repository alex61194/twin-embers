"""Twin Embers data-pack builder — offline graphical frontend (stdlib Tkinter).

No game ROM, pack, icon, network access or SDK component is bundled. The only
write action uses gui_service.build_private_pack and requires consent before
replacing a pre-existing output file.
"""
from __future__ import annotations

import queue
from pathlib import Path
import threading
import tkinter as tk
from tkinter import filedialog, messagebox, ttk

from .gui_service import BuildCancelled, build_private_pack, describe_error


class BuilderWindow(tk.Tk):
    def __init__(self) -> None:
        super().__init__()
        self.title("Twin Embers — Data Pack Builder")
        self.geometry("860x760")
        self.minsize(760, 720)
        self.configure(background="#fff6e8")
        self._events: queue.Queue = queue.Queue()
        self._cancel = threading.Event()
        self._busy = False
        self._closing = False
        self._create_dirs = False

        self.rom_path = tk.StringVar()
        self.pack_path = tk.StringVar()
        self.status = tk.StringVar(value="Ready. All files stay on this computer.")
        self.details = tk.StringVar(value="Choose your own supported FireRed ROM to begin.")
        self.phase = tk.StringVar(value="READY")

        style = ttk.Style(self)
        # Clam honors the palette on Windows instead of reverting to grey
        # native buttons. Focus rings and disabled states remain explicit.
        style.theme_use("clam")
        style.configure("Card.TFrame", background="#fffdf9")
        style.configure("Step.TLabel", background="#fffdf9", foreground="#721b20",
                        font=("Segoe UI", 12, "bold"))
        style.configure("Note.TLabel", background="#fffdf9", foreground="#60504a",
                        font=("Segoe UI", 10))
        style.configure("Status.TLabel", background="#fffdf9", foreground="#342b28",
                        font=("Segoe UI", 11, "bold"))
        style.configure("Footer.TLabel", background="#fff6e8", foreground="#60504a",
                        font=("Segoe UI", 9))
        style.configure("Action.TButton", padding=(22, 12), font=("Segoe UI", 11, "bold"),
                        background="#bd252d", foreground="white", bordercolor="#8f1820",
                        focuscolor="#ffc465")
        style.map("Action.TButton", background=[("disabled", "#eee3d7"),
                  ("pressed", "#8f1820"), ("active", "#d63832")],
                  foreground=[("disabled", "#776b64")])
        style.configure("Small.TButton", padding=(12, 9), font=("Segoe UI", 10, "bold"),
                        background="#fff0d5", foreground="#721b20", bordercolor="#d6b58d",
                        focuscolor="#bd252d")
        style.map("Small.TButton", background=[("disabled", "#eee3d7"),
                  ("pressed", "#f3c57d"), ("active", "#ffe0a6")],
                  foreground=[("disabled", "#776b64")])
        style.configure("Entry.TEntry", padding=9, fieldbackground="white",
                        foreground="#342b28", bordercolor="#d6b58d", font=("Segoe UI", 10))
        style.map("Entry.TEntry", bordercolor=[("focus", "#bd252d")],
                  fieldbackground=[("disabled", "#f2ece5")])
        style.configure("Ember.Horizontal.TProgressbar", background="#d63a2f",
                        troughcolor="#fff0d5", bordercolor="#d6b58d", lightcolor="#ef9e36",
                        darkcolor="#bd252d")

        heading = tk.Frame(self, background="#b82029", padx=28, pady=20)
        heading.pack(fill="x")
        tk.Label(heading, text="TWIN EMBERS", font=("Segoe UI", 26, "bold"),
                 background="#b82029", foreground="white", anchor="w").pack(fill="x")
        tk.Label(heading, text="FireRed Data Pack Builder", font=("Segoe UI", 13, "bold"),
                 background="#b82029", foreground="#ffe0a6", anchor="w").pack(fill="x")
        tk.Label(heading, text="NO GAME FILES INCLUDED", font=("Segoe UI", 9, "bold"),
                 background="#b82029", foreground="#fff4e6", anchor="w").pack(fill="x", pady=(8, 0))
        tk.Frame(self, background="#efaa42", height=4).pack(fill="x")

        body = tk.Frame(self, background="#fff6e8", padx=24, pady=18)
        body.pack(fill="both", expand=True)
        body.columnconfigure(0, weight=1)

        rom_card = self._card(body, 0)
        ttk.Label(rom_card, text="01   YOUR FIRERED ROM",
                  style="Step.TLabel").grid(row=0, column=0, sticky="w")
        rom_row = ttk.Frame(rom_card, style="Card.TFrame")
        rom_row.grid(row=1, column=0, sticky="ew", pady=(8, 4))
        rom_row.columnconfigure(0, weight=1)
        self.rom_entry = ttk.Entry(rom_row, textvariable=self.rom_path, style="Entry.TEntry")
        self.rom_entry.grid(row=0, column=0, sticky="ew", padx=(0, 8))
        self.rom_browse = ttk.Button(rom_row, text="Browse…", command=self._browse_rom,
                                     style="Small.TButton")
        self.rom_browse.grid(row=0, column=1)
        ttk.Label(rom_card, text="16 MiB, supported revision only. ROM verified automatically when building.",
                  style="Note.TLabel").grid(row=2, column=0, sticky="w")

        pack_card = self._card(body, 1)
        ttk.Label(pack_card, text="02   DATA PACK DESTINATION",
                  style="Step.TLabel").grid(row=0, column=0, sticky="w")
        pack_row = ttk.Frame(pack_card, style="Card.TFrame")
        pack_row.grid(row=1, column=0, sticky="ew", pady=(8, 4))
        pack_row.columnconfigure(0, weight=1)
        self.pack_entry = ttk.Entry(pack_row, textvariable=self.pack_path, style="Entry.TEntry")
        self.pack_entry.grid(row=0, column=0, sticky="ew", padx=(0, 8))
        self.pack_browse = ttk.Button(pack_row, text="Save as…", command=self._browse_output,
                                      style="Small.TButton")
        self.pack_browse.grid(row=0, column=1)
        self.sd_browse = ttk.Button(pack_row, text="SD folder…", command=self._browse_sd,
                                    style="Small.TButton")
        self.sd_browse.grid(row=0, column=2, padx=(8, 0))
        ttk.Label(pack_card, text="Save a .pak file or select your 3DS SD card's root folder.",
                  style="Note.TLabel").grid(row=2, column=0, sticky="w")

        build_card = self._card(body, 2)
        ttk.Label(build_card, text="03   BUILD & VERIFY",
                  style="Step.TLabel").grid(row=0, column=0, sticky="w")
        buttons = ttk.Frame(build_card, style="Card.TFrame")
        buttons.grid(row=1, column=0, sticky="w", pady=(12, 8))
        self.build_button = ttk.Button(buttons, text="Build Data Pack",
                                       command=self._build, style="Action.TButton")
        self.build_button.pack(side="left")
        self.cancel_button = ttk.Button(buttons, text="Cancel",
                                        command=self._request_cancel, state="disabled",
                                        style="Small.TButton")
        self.cancel_button.pack(side="left", padx=(12, 0))

        self.indicator = ttk.Progressbar(build_card, mode="indeterminate",
                                         style="Ember.Horizontal.TProgressbar")
        self.indicator.grid(row=2, column=0, sticky="ew", pady=(4, 10))
        self.indicator.grid_remove()
        self.phase_label = tk.Label(build_card, textvariable=self.phase,
                                    font=("Segoe UI", 9, "bold"), padx=9, pady=4,
                                    background="#fff0d5", foreground="#721b20")
        self.phase_label.grid(row=3, column=0, sticky="w", pady=(3, 7))
        self.status_label = ttk.Label(build_card, textvariable=self.status,
                                      style="Status.TLabel", wraplength=690)
        self.status_label.grid(row=4, column=0, sticky="ew")
        self.details_label = ttk.Label(build_card, textvariable=self.details,
                                       style="Note.TLabel", wraplength=690)
        self.details_label.grid(row=5, column=0, sticky="ew", pady=(5, 0))
        build_card.bind("<Configure>", self._wrap_status)

        ttk.Label(body, text="Local processing only. Nothing is uploaded.\n"
                  "No game files included. Creates a data pack, not a 3DS game executable.",
                  style="Footer.TLabel", wraplength=700).grid(
                      row=3, column=0, sticky="w", pady=(5, 0))

        self.protocol("WM_DELETE_WINDOW", self._close)
        self.after(80, self._poll)
        self.rom_entry.focus_set()
        # Button.invoke respects disabled state, including keyboard shortcuts.
        for key, button in (("<Alt-b>", self.build_button),
                            ("<Escape>", self.cancel_button)):
            self.bind(key, lambda event, action=button: action.invoke())

    def _card(self, parent: tk.Frame, row: int) -> ttk.Frame:
        border = tk.Frame(parent, background="#dfc3a4", padx=1, pady=1)
        border.grid(row=row, column=0, sticky="ew", pady=(0, 12))
        card = ttk.Frame(border, style="Card.TFrame", padding=(18, 14))
        card.pack(fill="both", expand=True)
        card.columnconfigure(0, weight=1)
        return card

    def _wrap_status(self, event: tk.Event) -> None:
        width = max(200, event.width - 36)
        self.status_label.configure(wraplength=width)
        self.details_label.configure(wraplength=width)

    def _set_phase(self, text: str, color: str = "#721b20") -> None:
        self.phase.set(text)
        self.phase_label.configure(foreground=color)

    def _browse_rom(self) -> None:
        path = filedialog.askopenfilename(parent=self, title="Choose your own FireRed ROM",
                                          filetypes=[("GBA ROM", "*.gba"), ("All files", "*.*")])
        if path:
            self.rom_path.set(path)
            if not self.pack_path.get():
                self.pack_path.set(str(Path(path).parent / "twinembers.pak"))
                self._create_dirs = False
            self.status.set("ROM selected. Choose a destination and build the pack.")

    def _browse_output(self) -> None:
        path = filedialog.asksaveasfilename(
            parent=self, title="Choose the data-pack destination",
            defaultextension=".pak", initialfile="twinembers.pak",
            filetypes=[("FireRed data pack", "*.pak")])
        if path:
            self.pack_path.set(path)
            self._create_dirs = False

    def _browse_sd(self) -> None:
        root = filedialog.askdirectory(parent=self, title="Select the root folder of your 3DS SD card")
        if root:
            self.pack_path.set(str(Path(root) / "3ds" / "twinembers" / "twinembers.pak"))
            self._create_dirs = True

    def _set_busy(self, busy: bool) -> None:
        self._busy = busy
        state = "disabled" if busy else "normal"
        for widget in (self.rom_entry, self.pack_entry, self.rom_browse,
                       self.pack_browse, self.sd_browse,
                       self.build_button):
            widget.configure(state=state)
        self.cancel_button.configure(state="normal" if busy else "disabled")
        if busy:
            self.indicator.grid()
            self.indicator.start(12)
            self._set_phase("WORKING")
        else:
            self.indicator.stop()
            self.indicator.grid_remove()

    def _start(self, rom: str, target: str, overwrite: bool = False) -> None:
        self._cancel = threading.Event()
        self._set_busy(True)
        self.status.set("Preparing local operation…")
        self.details.set("Do not remove the destination drive while writing the pack.")

        def worker() -> None:
            try:
                report = build_private_pack(
                    rom, target, overwrite=overwrite,
                    create_directories=self._create_dirs,
                    on_progress=lambda step, msg: self._events.put(("progress", step, msg)),
                    cancel=self._cancel)
                self._events.put(("ok_build", report))
            except BuildCancelled as exc:
                self._events.put(("cancelled", str(exc)))
            except (ValueError, OSError, KeyError, TypeError, IndexError) as exc:
                self._events.put(("error", describe_error(exc)))
            except Exception as exc:
                self._events.put(("error", "Unexpected builder failure: " + str(exc)))

        threading.Thread(target=worker, name="pack-builder", daemon=False).start()

    def _build(self) -> None:
        if self._busy:
            return
        rom = self.rom_path.get().strip()
        target = self.pack_path.get().strip()
        if not rom or not target:
            messagebox.showwarning("Paths required", "Choose both a ROM and a pack destination.", parent=self)
            return
        dest = Path(target).expanduser()
        overwrite = False
        if dest.is_symlink():
            messagebox.showerror("Unsafe output", "The output may not be a symbolic link.", parent=self)
            return
        if dest.exists():
            if not dest.is_file():
                messagebox.showerror("Invalid output", "That destination is not a file.", parent=self)
                return
            overwrite = messagebox.askyesno(
                "Replace existing pack?",
                "This destination already contains a file.\n\n"
                + str(dest)
                + "\n\nThe previous file will be kept until the new pack is verified. "
                  "Do you explicitly authorize replacing it?",
                icon="warning", default="no", parent=self)
            if not overwrite:
                self.status.set("Cancelled: existing pack preserved.")
                return
        self._start(rom, target, overwrite=overwrite)

    def _request_cancel(self) -> None:
        if self._busy:
            self._cancel.set()
            self.cancel_button.configure(state="disabled")
            self._set_phase("CANCELLING")
            self.status.set("Cancellation requested; waiting for the current build stage…")

    def _poll(self) -> None:
        try:
            while True:
                msg = self._events.get_nowait()
                kind = msg[0]
                if kind == "progress":
                    if not self._cancel.is_set():
                        self.status.set(msg[2])
                        self.details.set("Stage: " + msg[1]
                                         + "  |  No files are sent over a network.")
                else:
                    self._set_busy(False)
                    if kind == "ok_build":
                        self._set_phase("PACK VERIFIED", "#28613b")
                        report = msg[1]
                        self.status.set("Build complete. Data pack verified.")
                        self.details.set("Entries: {0:,}  |  ABI: {1:08x}  |  SHA-256: {2}".format(
                            report["entries"], report["abi"], report["sha256"]))
                        messagebox.showinfo("Build complete",
                                            "Verified data pack saved to:\n"
                                            + report["path"] + "\n\n"
                                            + "Entries: {0:,}\nSHA-256: {1}".format(
                                                report["entries"], report["sha256"])
                                            + "\n\nNext: copy twinembers.3dsx into the same folder.",
                                            parent=self)
                    elif kind == "cancelled":
                        self._set_phase("CANCELLED")
                        self.status.set("Cancelled safely. No new pack was installed.")
                        self.details.set(msg[1])
                    else:
                        self._set_phase("CHECK REQUIRED", "#a31d26")
                        self.status.set("Operation failed. Check the details before trying again.")
                        self.details.set(msg[1])
                        messagebox.showerror("Builder error", msg[1], parent=self)
                    if self._closing:
                        self.destroy()
                        return
        except queue.Empty:
            pass
        self.after(80, self._poll)

    def _close(self) -> None:
        if not self._busy:
            self.destroy()
            return
        self._closing = True
        self._request_cancel()
        self.status.set("Finishing safely before closing. Please wait…")


def main() -> None:
    BuilderWindow().mainloop()


if __name__ == "__main__":
    main()
