---
layout: default
title: Get MMapper for macOS
---

To install MMapper on macOS, download the **Disk Image (DMG)** file provided below.

MMapper is a universal binary, meaning it runs natively on both older **Intel-based Macs** and the latest **Apple Silicon (M1, M2, and M3)** models. After downloading, simply open the DMG file and drag MMapper to your Applications folder.

{% for asset in site.github.latest_release.assets %}
{% if asset.name contains 'sha256' %}
{% elsif asset.name contains 'dmg' %}
<a href="{{ asset.browser_download_url }}" class="download-link">
    Download {{ asset.name }}
</a>{% assign asset_name_lower = asset.name | downcase %}{% capture arch_label %}{% if asset_name_lower contains 'arm64' %}Apple Silicon{% elsif asset_name_lower contains 'x86_64' %}Intel{% endif %}{% endcapture %}{% if arch_label != "" %} <span class="arch-label">{{ arch_label }}</span>{% endif %}
{% endif %}
{% endfor %}

<div class="notice-box" id="macos-notice">
  <strong style="color: #d9534f;">Important Notice for macOS Users:</strong><br>
  On newer versions of macOS (Sequoia and later), you might see a message stating that the app "cannot be opened because it is from an unidentified developer."<br>
  <strong>To allow MMapper to run:</strong>
  <ol>
    <li>Open <strong>System Settings</strong>.</li>
    <li>Navigate to <strong>Privacy & Security</strong>.</li>
    <li>Scroll down to the "Security" section.</li>
    <li>You should see a message about MMapper being blocked. Click <strong>Open Anyway</strong>.</li>
    <li>Enter your password or use Touch ID when prompted.</li>
  </ol>
</div>

{% include download.md %}
