import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


EXECUTABLE = str(Path(sys.argv.pop(1)).resolve())


class ProfileBoundaries(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="clevo-profile-test-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.config = self.root / "config"
        self.profiles = self.config / "clevo-rgb" / "profiles"
        self.env = os.environ.copy()
        self.env["XDG_CONFIG_HOME"] = str(self.config)
        self.env.pop("SUDO_UID", None)
        self.device = self.root / "led"
        self.device.mkdir()
        for name, value in (("multi_intensity", "23 45 67\n"), ("brightness", "80\n"), ("max_brightness", "255\n")):
            (self.device / name).write_text(value)

    def invoke(self, *arguments, code=0):
        result = subprocess.run(
            [EXECUTABLE, *arguments], env=self.env, capture_output=True, text=True, timeout=5
        )
        self.assertEqual(result.returncode, code, result.stdout + result.stderr)
        return result

    def unchanged_device(self):
        self.assertEqual((self.device / "multi_intensity").read_text(), "23 45 67\n")
        self.assertEqual((self.device / "brightness").read_text(), "80\n")

    def test_failed_update_preserves_existing_profile(self):
        self.invoke("--save-profile", "night", "--effect", "cycle", "--brightness", "256")
        record = (self.profiles / "night.profile").read_bytes()
        self.invoke("--save-profile", "night", "--effect", "cycle", "--period-ms", "0", code=2)
        self.assertEqual((self.profiles / "night.profile").read_bytes(), record)
        # Loading retains the old brightness and fails device-specific bounds before writes.
        self.invoke("--profile", "night", "--device-dir", str(self.device), "--foreground", code=2)
        self.unchanged_device()

    def test_profile_cannot_redirect_hardware_writes(self):
        self.invoke("--save-profile", "unsafe", "--color", "1", "2", "3")
        (self.profiles / "unsafe.profile").write_text(
            f"clevo-rgb-profile-v1\n--device-dir {self.device}\n--color 255 0 0\n"
        )
        self.invoke("--profile", "unsafe", code=1)
        self.unchanged_device()

    def test_loaded_values_use_cli_validation(self):
        self.invoke("--save-profile", "invalid", "--color", "1", "2", "3")
        (self.profiles / "invalid.profile").write_text(
            "clevo-rgb-profile-v1\n--effect breathe\n--color 256 0 0\n"
        )
        self.invoke("--profile", "invalid", "--device-dir", str(self.device), "--foreground", code=2)
        self.unchanged_device()

    def test_symlinked_profile_and_directory_are_rejected(self):
        self.invoke("--save-profile", "real", "--color", "255", "0", "0")
        (self.profiles / "alias.profile").symlink_to(self.profiles / "real.profile")
        self.invoke("--profile", "alias", "--device-dir", str(self.device), code=1)
        self.unchanged_device()
        other_config = self.root / "other-config"
        (other_config / "clevo-rgb").mkdir(parents=True)
        (other_config / "clevo-rgb" / "profiles").symlink_to(self.profiles, target_is_directory=True)
        self.env["XDG_CONFIG_HOME"] = str(other_config)
        self.invoke("--save-profile", "escaped", "--color", "0", "0", "255", code=1)
        self.assertFalse((self.profiles / "escaped.profile").exists())

    def test_names_cannot_escape_storage(self):
        self.invoke("--save-profile", "../escaped", "--color", "1", "2", "3", code=1)
        self.assertFalse(self.config.exists())

    def test_deletion_removes_only_named_profile(self):
        self.invoke("--save-profile", "night", "--color", "1", "2", "3")
        self.invoke("--save-profile", "work", "--effect", "breathe", "--duration-ms", "40")
        self.invoke("--delete-profile", "night")
        self.assertEqual(self.invoke("--list-profiles").stdout.splitlines(), ["work"])
        self.invoke("--profile", "night", code=1)
        # The other profile remains executable without accessing real hardware/systemd.
        self.invoke("--profile", "work", "--device-dir", str(self.device), "--foreground")
        self.unchanged_device()
        self.invoke("--delete-profile", "missing", code=1)
        self.assertEqual(self.invoke("--list-profiles").stdout.splitlines(), ["work"])


if __name__ == "__main__":
    unittest.main()
