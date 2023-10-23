class Keymapper < Formula
  desc "Cross-platform context-aware keyremapper"
  homepage "https://github.com/houmain/keymapper"
  head "https://github.com/houmain/keymapper.git", branch: "next"
  license "GPL-3.0-only"

  depends_on "cmake" => :build
  on_linux do
    depends_on "dbus"
  end

  def install
    system "cmake", "-B build", *std_cmake_args
    system "cmake", "--build", "build"
    system "cmake", "--install", "build"
  end

  def caveats
    <<~EOS
      To add keymapperd and keymapper to the launchd daemons/agents call:
      `sudo keymapper-launchd add`
      to remove them call:
      `sudo keymapper-launchd remove`
    EOS
  end

  test do
    system "echo", "'A >> B' > #{bin}/test.conf"
    system "#{bin}/keymapper", "--check", "--config", "test.conf"
    system "#{bin}/keymapperd", "--help"
  end
end
