"""Start the offline Tk GUI; --self-test checks frozen recipe without a display."""
import sys


def _self_test():
    from firered3ds_builder.build import default_recipe
    from firered3ds_builder.recipe import load_recipe
    from firered3ds_builder.rom import SUPPORTED_SHA1

    recipe = load_recipe(default_recipe(SUPPORTED_SHA1))
    assert recipe["rom_sha1"] == SUPPORTED_SHA1
    assert recipe["engine_abi"] == 0x8F71CF3A
    assert len(recipe["entries"]) == 6734
    assert len(recipe["units"]) == 12211


if __name__ == "__main__":
    if sys.argv[1:] == ["--self-test"]:
        _self_test()
    elif len(sys.argv) == 1:
        from firered3ds_builder.gui import main
        main()
    else:
        raise SystemExit("Unknown arguments; only --self-test is supported.")
