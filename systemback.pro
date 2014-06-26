TEMPLATE = subdirs

SUBDIRS += libsystemback \
  sbscheduler \
  sbsysupgrade \
  systemback \
  systemback-cli

sbscheduler.depends = libsystemback
sbsysupgrade.depends = libsystemback
systemback.depends = libsystemback
systemback-cli.depends = libsystemback

TRANSLATIONS = lang/systemback_hu.ts \
    lang/systemback_ar.ts \
    lang/systemback_ca.ts \
    lang/systemback_es.ts \
    lang/systemback_fi.ts \
    lang/systemback_fr.ts \
    lang/systemback_gl_ES.ts \
    lang/systemback_pt.ts \
    lang/systemback_ro.ts \
    lang/systemback_zh.ts
