document.addEventListener('DOMContentLoaded', function() {
    let detectedArch = null;
    let isChromeOS = false;
    const downloadLinks = document.querySelectorAll('.download-link, .platform-link');

    // --- Architecture Detection (Best Effort) ---
    if (navigator.userAgentData && navigator.userAgentData.architecture) {
        detectedArch = navigator.userAgentData.architecture.toLowerCase();
    } else {
        const ua = navigator.userAgent.toLowerCase();
        if (ua.includes('x86_64') || ua.includes('amd64') || ua.includes('win64')) {
            detectedArch = 'x86_64';
        } else if (ua.includes('arm64') || ua.includes('aarch64')) {
            detectedArch = 'arm64';
        }
    }

    if (navigator.userAgentData && navigator.userAgentData.platform) {
        const platform = navigator.userAgentData.platform.toLowerCase();
        if (platform.includes('chromeos') || platform.includes('cros')) {
            isChromeOS = true;
        }
    }

    // --- Highlight Download Link ---
    function addRecommendation(link) {
        link.classList.add('recommended-download');
        const recommendation = document.createElement('span');
        recommendation.textContent = ' Recommended';
        recommendation.classList.add('recommendation-text');

        if (link.classList.contains('platform-link')) {
            // For homepage icons, put it inside so it doesn't break flex layout
            recommendation.classList.add('platform-recommendation');
            link.appendChild(recommendation);
        } else {
            link.parentNode.insertBefore(recommendation, link.nextSibling);
        }
    }

    downloadLinks.forEach(link => {
        const href = link.href.toLowerCase();

        // Do not recommend Windows .exe installers
        if (href.includes('.exe')) {
            return;
        }

        // Recommend Web/PWA for ChromeOS
        if (isChromeOS && href.includes('/demo/')) {
            addRecommendation(link);
            return;
        }

        let linkArch = null;
        if (href.includes('x86_64') || href.includes('amd64') || href.includes('x64')) {
            linkArch = 'x86_64';
        } else if (href.includes('arm64') || href.includes('aarch64')) {
            linkArch = 'arm64';
        }

        if (detectedArch && linkArch === detectedArch) {
            addRecommendation(link);
        }
    });
});
