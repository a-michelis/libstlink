###
# Documentation
#
# Two outputs from two sources, and neither is needed to build the library.
#
#   man/stlink.3.md   -> stlink.3, the library's manual page
#   src/*.md          -> site/, the documentation site
#
# The manual page is committed in generated form beside its source, and that
# is what gets installed. An ordinary build therefore installs a manual page
# without pandoc, without mkdocs and without python, which is the point: a
# person building this library should not have to acquire a documentation
# toolchain to end up with a working `man stlink`.
#
# STLINK_BUILD_DOCS regenerates both. That is a maintainer's switch, used when
# the sources change, and the regenerated manual page is then committed.
###

include(GNUInstallDirs)

set(STLINK_MAN_PAGE stlink.3)
set(STLINK_MAN_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/man/${STLINK_MAN_PAGE}.md)
set(STLINK_MAN_COMMITTED ${CMAKE_CURRENT_SOURCE_DIR}/man/${STLINK_MAN_PAGE})

###
# What an ordinary build does: install what is already there.
###
if (EXISTS ${STLINK_MAN_COMMITTED})
    install(FILES ${STLINK_MAN_COMMITTED}
            DESTINATION ${CMAKE_INSTALL_MANDIR}/man3)
else ()
    # Only reachable in a working tree where the generated page has been
    # removed but not rebuilt. Worth saying out loud rather than quietly
    # installing nothing.
    message(AUTHOR_WARNING
            "${STLINK_MAN_PAGE} is not present; configure with -DSTLINK_BUILD_DOCS=ON "
            "and build the docs target to generate it")
endif()

if (NOT STLINK_BUILD_DOCS)
    return()
endif()

###
# From here down is the maintainer's path.
###

find_program(STLINK_PANDOC pandoc)

###
# Finding mkdocs is fiddlier than it looks, and worth doing properly.
#
# It is a python module whose console script is often not on PATH even when the
# module imports perfectly, which is the normal situation on Windows. And a
# machine may carry several pythons, only one of which has it: CMake's own
# finder picks by version and will happily hand back the one without it.
#
# So try the console script, then every interpreter we can name, and ask each
# whether it actually has the module rather than assuming.
#
# Set -DSTLINK_MKDOCS=<path> to override all of this.
###
find_program(STLINK_MKDOCS mkdocs)

if (NOT STLINK_MKDOCS)
    find_package(Python3 QUIET COMPONENTS Interpreter)
    find_program(STLINK_PYTHON_ON_PATH NAMES python3 python)

    foreach (candidate ${Python3_EXECUTABLE} ${STLINK_PYTHON_ON_PATH})
        if (STLINK_MKDOCS)
            break()
        endif()

        execute_process(COMMAND ${candidate} -m mkdocs --version
                        RESULT_VARIABLE STLINK_MKDOCS_ABSENT
                        OUTPUT_QUIET ERROR_QUIET)

        if (STLINK_MKDOCS_ABSENT EQUAL 0)
            set(STLINK_MKDOCS ${candidate} -m mkdocs)
        endif()
    endforeach()
endif()

add_custom_target(docs)

###
# The manual page.
#
# Written straight back into the source tree rather than into the build
# directory, because the generated page is a committed artefact: building it
# is how you update it. That is the same arrangement the C project used, and
# it is what lets everyone else build without pandoc.
###
if (STLINK_PANDOC)
    add_custom_target(docs-man
            COMMAND ${STLINK_PANDOC} --standalone --to man
                    ${STLINK_MAN_SOURCE} --output ${STLINK_MAN_COMMITTED}
            DEPENDS ${STLINK_MAN_SOURCE}
            BYPRODUCTS ${STLINK_MAN_COMMITTED}
            COMMENT "Generating ${STLINK_MAN_PAGE} from ${STLINK_MAN_PAGE}.md"
            VERBATIM)

    add_dependencies(docs docs-man)

    message(STATUS "Documentation: manual page via ${STLINK_PANDOC}")
else ()
    message(STATUS "Documentation: pandoc not found, the manual page cannot be regenerated")
endif()

###
# The site.
#
# Into the build directory, because unlike the manual page this is not a
# committed artefact: it is published by CI and read from a URL.
###
if (STLINK_MKDOCS)
    add_custom_target(docs-site
            COMMAND ${STLINK_MKDOCS} build --strict
                    --config-file ${CMAKE_CURRENT_SOURCE_DIR}/mkdocs.yml
                    --site-dir ${CMAKE_CURRENT_BINARY_DIR}/site
            COMMENT "Building the documentation site"
            VERBATIM)

    add_dependencies(docs docs-site)

    message(STATUS "Documentation: site via ${STLINK_MKDOCS}")
else ()
    message(STATUS "Documentation: mkdocs not found, the site cannot be built "
                   "(pip install -r docs/requirements.txt)")
endif()
