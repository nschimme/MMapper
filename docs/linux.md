---
layout: default
title: Get MMapper for Linux
---

MMapper is available on Linux through several popular packaging formats. We highly recommend using **Flathub (Flatpak)** as it provides the most consistent experience across different Linux distributions, including automatic updates and a secure sandbox.

For those on Ubuntu or other Snap-enabled systems, the **Snap Store** is also a great option. Additionally, we provide a standalone **AppImage** that can be run directly without installation.

<a href='https://flathub.org/apps/org.mume.MMapper'>
    <img width='200' alt='Get it on Flathub' src='https://flathub.org/api/badge?locale=en' style="vertical-align: middle;"/>
</a>

[![Get it from the Snap Store](https://snapcraft.io/en/dark/install.svg)](https://snapcraft.io/mmapper)

{% for asset in site.github.latest_release.assets %}
{% if asset.name contains 'sha256' %}
{% elsif asset.name contains 'AppImage' %}
<a href="{{ asset.browser_download_url }}" class="download-link">
    Download {{ asset.name }}
</a>
{% endif %}
{% endfor %}

{% include download.md %}
