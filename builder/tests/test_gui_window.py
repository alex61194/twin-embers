"""Exercise the actual GUI methods with a display-free Tk simulation."""
import ast
from pathlib import Path
import queue
import re
import sys
import threading
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch

from firered3ds_builder import gui_service

SOURCE = Path(__file__).resolve().parents[1] / "firered3ds_builder" / "gui.py"


class Widget:
    def __init__(self, parent=None, **kwargs):
        self.options = kwargs
        self.visible = False
        self.running = False
        self.children = []
        self.bindings = {}
        if parent is not None:
            parent.children.append(self)

    def configure(self, **kwargs):
        self.options.update(kwargs)

    def grid(self, **kwargs):
        self.visible = True
        self.options.update(kwargs)

    def grid_remove(self):
        self.visible = False

    def pack(self, **kwargs):
        self.visible = True

    def columnconfigure(self, *args, **kwargs):
        pass

    def bind(self, key, callback):
        self.bindings[key] = callback

    def invoke(self):
        if self.options.get("state") != "disabled":
            self.options["command"]()

    def start(self, interval):
        self.running = True

    def stop(self):
        self.running = False

    def after(self, *args):
        pass

    def protocol(self, *args):
        pass

    def title(self, text):
        self.options["title"] = text

    def geometry(self, *args):
        pass

    def minsize(self, *args):
        pass

    def focus_set(self):
        self.focused = True

    def destroy(self):
        self.destroyed = True


class Variable:
    def __init__(self, value=""):
        self.value = value

    def get(self):
        return self.value

    def set(self, value):
        self.value = value


def simulated_window():
    # Execute the actual class, removing only imports and the script entrypoint.
    # This works on Linux even when Tcl/Tk is not installed.
    tree = ast.parse(SOURCE.read_text(encoding="utf-8"))
    tree.body = [node for node in tree.body if isinstance(node, ast.ClassDef)]
    tk = SimpleNamespace(Tk=Widget, Frame=Widget, Label=Widget,
                         StringVar=Variable, Event=object)
    ttk = SimpleNamespace(Style=Mock(), Frame=Widget, Label=Widget,
                          Entry=Widget, Button=Widget, Progressbar=Widget)
    namespace = dict(tk=tk, ttk=ttk, queue=queue, threading=threading, Path=Path,
                     filedialog=Mock(), messagebox=Mock(),
                     BuildCancelled=gui_service.BuildCancelled,
                     build_private_pack=Mock())
    exec(compile(tree, str(SOURCE), "exec"), namespace)
    return namespace["BuilderWindow"](), namespace


class GuiWindowTest(unittest.TestCase):
    def setUp(self):
        self.window, self.namespace = simulated_window()

    def test_initial_progress_is_hidden_and_stopped(self):
        self.assertFalse(self.window.indicator.visible)
        self.assertFalse(self.window.indicator.running)
        self.assertTrue(self.window.rom_entry.focused)

    def test_sd_selection_uses_exact_twinembers_destination(self):
        self.namespace['filedialog'].askdirectory.return_value = 'synthetic-sd'
        self.window._browse_sd()
        self.assertEqual(Path(self.window.pack_path.get()),
                         Path('synthetic-sd/3ds/twinembers/twinembers.pak'))
        self.assertTrue(self.window._create_dirs)

    def test_rom_selection_defaults_pack_name_and_keeps_chosen_sd_path(self):
        self.namespace['filedialog'].askopenfilename.return_value = 'roms/own.gba'
        self.window._browse_rom()
        self.assertEqual(Path(self.window.pack_path.get()), Path('roms/twinembers.pak'))
        self.namespace['filedialog'].askdirectory.return_value = 'synthetic-sd'
        self.window._browse_sd()
        self.window._browse_rom()
        self.assertEqual(Path(self.window.pack_path.get()),
                         Path('synthetic-sd/3ds/twinembers/twinembers.pak'))

    def test_build_shows_progress_and_disables_inputs(self):
        with patch.object(threading, "Thread"):
            self.window._start("synthetic.gba", "synthetic.pak")
            self.assertTrue(self.window.indicator.visible)
            self.assertTrue(self.window.indicator.running)
            self.assertEqual(self.window.rom_entry.options["state"], "disabled")
            self.assertEqual(self.window.cancel_button.options["state"], "normal")
            self.window._set_busy(False)

    def test_every_terminal_event_hides_progress_and_restores_controls(self):
        report = dict(entries=1, abi=0, sha256="synthetic", path="synthetic.pak")
        for event in (("ok_build", report),
                      ("cancelled", "Cancelled"), ("error", "Synthetic failure")):
            with self.subTest(event=event[0]):
                self.window._set_busy(True)
                self.window._events.put(event)
                self.window._poll()
                self.assertFalse(self.window.indicator.visible)
                self.assertFalse(self.window.indicator.running)
                self.assertFalse(self.window._busy)
                self.assertEqual(self.window.build_button.options["state"], "normal")
                self.assertEqual(self.window.cancel_button.options["state"], "disabled")

    def test_cancel_waits_for_worker_before_hiding_progress(self):
        self.window._set_busy(True)
        self.window._request_cancel()
        self.assertTrue(self.window._cancel.is_set())
        self.assertTrue(self.window.indicator.running)
        self.window._events.put(("cancelled", "Cancelled"))
        self.window._poll()
        self.assertFalse(self.window.indicator.visible)

    def test_close_waits_safely_for_terminal_event(self):
        self.window._set_busy(True)
        self.window._close()
        self.assertTrue(self.window._closing)
        self.assertFalse(getattr(self.window, "destroyed", False))
        self.window._events.put(("cancelled", "Cancelled"))
        self.window._poll()
        self.assertTrue(self.window.destroyed)
        self.assertFalse(self.window.indicator.visible)

    def test_missing_paths_and_reentrant_calls_do_not_start_work(self):
        self.window._build()
        self.assertFalse(self.window.indicator.visible)
        self.window._set_busy(True)
        with patch.object(threading, "Thread") as worker:
            self.window._build()
            self.assertFalse(worker.called)

    def test_keyboard_shortcuts_respect_busy_state(self):
        self.assertNotIn("<Alt-v>", self.window.bindings)
        self.assertFalse(hasattr(self.window, "verify_button"))
        build = Mock()
        self.window.build_button.options["command"] = build
        self.window.bindings["<Alt-b>"](None)
        build.assert_called_once()
        self.window._set_busy(True)
        self.window.bindings["<Alt-b>"](None)
        build.assert_called_once()
        self.window.bindings["<Escape>"](None)
        self.assertTrue(self.window._cancel.is_set())

    def test_ui_strings_have_no_private_label(self):
        tree = ast.parse(SOURCE.read_text(encoding="utf-8"))
        for node in ast.walk(tree):
            if isinstance(node, ast.Constant) and isinstance(node.value, str):
                # Exclude the module documentation, which names the service API.
                if node is tree.body[0].value:
                    continue
                self.assertIsNone(re.search(r"\bprivate\b", node.value, re.I), node.value)

    def test_picker_titles_and_offline_notice(self):
        self.window._browse_output()
        self.assertEqual(self.namespace["filedialog"].asksaveasfilename.call_args.kwargs["title"],
                         "Choose the data-pack destination")
        source = SOURCE.read_text(encoding="utf-8")
        self.assertIn("NO GAME FILES INCLUDED", source)
        self.assertIn("Nothing is uploaded", source)


@unittest.skipUnless(sys.platform == "win32", "Native Tk smoke test runs on Windows")
class NativeWindowsGuiTest(unittest.TestCase):
    def test_native_layout_focus_and_progress(self):
        from firered3ds_builder.gui import BuilderWindow
        window = BuilderWindow()
        try:
            window.update()
            self.assertFalse(window.indicator.winfo_ismapped())
            for busy in (True, False, True, False):
                window._set_busy(busy)
                window.update()
                self.assertEqual(bool(window.indicator.winfo_ismapped()), busy)
            # Check the minimum size with the indicator visible and no content
            # clipped below the window. Includes real ttk requested dimensions.
            window.geometry("760x720")
            window._set_busy(True)
            window.update()
            for widget in (window.rom_entry, window.pack_entry, window.build_button,
                           window.cancel_button, window.status_label, window.details_label):
                self.assertGreater(widget.winfo_width(), 0)
                self.assertLessEqual(widget.winfo_rooty() + widget.winfo_height(),
                                     window.winfo_rooty() + window.winfo_height())
            window.rom_entry.focus_force()
            window.update()
            self.assertIs(window.focus_get(), window.rom_entry)
        finally:
            window.destroy()


if __name__ == "__main__":
    unittest.main()
