import sys

def fix_file(filename):
    with open(filename, 'r') as f:
        content = f.read()

    replacements = [
        ('void Configuration::CanvasSettings::Advanced::registerChangeCallback(\n    const ChangeMonitor::Lifetime &lifetime, const ChangeMonitor::Function &callback)',
         'void Configuration::CanvasSettings::Advanced::registerChangeCallback(\n    const ChangeMonitor::Lifetime &lifetime, const ChangeMonitor::Function &callback) const'),
        ('void Configuration::CanvasSettings::registerChangeCallback(const ChangeMonitor::Lifetime &lifetime,\n                                                           const ChangeMonitor::Function &callback)',
         'void Configuration::CanvasSettings::registerChangeCallback(const ChangeMonitor::Lifetime &lifetime,\n                                                           const ChangeMonitor::Function &callback) const'),
        ('void Configuration::NamedColorOptions::registerChangeCallback(\n    const ChangeMonitor::Lifetime &lifetime, const ChangeMonitor::Function &callback)',
         'void Configuration::NamedColorOptions::registerChangeCallback(\n    const ChangeMonitor::Lifetime &lifetime, const ChangeMonitor::Function &callback) const'),
        ('void Configuration::registerChangeCallback(const ChangeMonitor::Lifetime &lifetime,\n                                           const ChangeMonitor::Function &callback)',
         'void Configuration::registerChangeCallback(const ChangeMonitor::Lifetime &lifetime,\n                                           const ChangeMonitor::Function &callback) const')
    ]

    for old, new in replacements:
        content = content.replace(old, new)

    with open(filename, 'w') as f:
        f.write(content)

fix_file('src/configuration/configuration.cpp')
