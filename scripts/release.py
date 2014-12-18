#!/usr/env/python
# Usage: release.py VERSION. Creates a new compiz release and bumps VERSION
# to the next release
#
# Copyright (c) Sam Spilsbury <smspillaz@gmail.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
import sys
import os
import bzrlib.branch
import bzrlib.workingtree
import bzrlib.errors as errors
from launchpadlib.launchpad import Launchpad
import datetime

def usage ():
    print ("release.py VERSION [SINCE]")
    print ("Make a release and bump version to VERSION")
    sys.exit (1)

if len(sys.argv) < 2:
    usage ()

if not "release.py" in os.listdir ("."):
    print ("Working directory must contain this script")
    sys.exit (1)

editor = ""

try:
    editor = os.environ["EDITOR"]
except KeyError:
    print ("EDITOR must be set")
    sys.exit (1)

if len (editor) == 0:
    print ("EDITOR must be set")
    sys.exit (1)

bzrlib.initialize ()
compiz_branch = bzrlib.branch.Branch.open ("..")
compiz_branch.lock_read ()
tags = compiz_branch.tags.get_tag_dict ().items ()

most_recent_revision = 0

since = None

if len (sys.argv) == 3:
    since = sys.argv[2];

# Find the last tagged revision or the specified tag
for tag, revid in tags:
    try:
        revision = compiz_branch.revision_id_to_dotted_revno (revid)[0]

        if since != None:
            if tag == since:
                most_recent_revision = revision
 
        elif int (revision) > most_recent_revision:
            most_recent_revision = revision

    except (errors.NoSuchRevision,
            errors.GhostRevisionsHaveNoRevno,
            errors.UnsupportedOperation):
        pass

repo = compiz_branch.repository
release_revisions = []

# Find all revisions since that one
for revision_id in repo.get_revisions (repo.all_revision_ids ()):
    try:
        revno = compiz_branch.revision_id_to_dotted_revno (revision_id.revision_id)[0]

        if revno > most_recent_revision:
            release_revisions.append (revision_id)

    except (errors.NoSuchRevision,
            errors.GhostRevisionsHaveNoRevno,
            errors.UnsupportedOperation):
        pass

# Find all fixed bugs in those revisions
bugs = []

for rev in release_revisions:
    for bug in rev.iter_bugs():
        bugs.append (bug[0][27:])

bugs = sorted (bugs)

# Connect to launchpad
lp = Launchpad.login_anonymously ("Compiz Release Script", "production")

# Create a pretty looking formatted list of bugs
bugs_formatted = []

for bug in bugs:
    lpBug = lp.bugs[bug]
    bugTitle = lpBug.title

    bugTitleWords = bugTitle.split (" ")
    maximumLineLength = 65
    currentLineLength = 0
    line = 0
    lineParts = [""]

    for word in bugTitleWords:
        wordLength = len (word) + 1
        if wordLength + currentLineLength > maximumLineLength:
            lineParts.append ("")
            line = line + 1
            currentLineLength = 0
        elif currentLineLength != 0:
            lineParts[line] += " "

        currentLineLength += wordLength
        lineParts[line] += (word)

    bugs_formatted.append ((bug, lineParts))

# Pretty-print the bugs
bugs_formatted_msg = ""

for bug, lines in bugs_formatted:
    whitespace = " " * (12 - len (bug))
    bugs_formatted_msg += whitespace + bug + " - " + lines[0] + "\n"

    if len (lines) > 1:
        for i in range (1, len (lines)):
            whitespace = " " * 15
            bugs_formatted_msg += whitespace + lines[i] + "\n"

# Get the version
version_file = open ("../VERSION", "r")
version = version_file.readlines ()[0][:-1]
version_file.close ()

# Get the date
date = datetime.datetime.now ()
date_formatted = str(date.year) + "-" + str(date.month) + "-" + str(date.day)

# Create release message
release_msg = "Release " + version + " (" + date_formatted + " NAME <EMAIL>)\n"
release_msg += "=" * 77 + "\n\n"
release_msg += "Highlights\n\n"
release_msg += "Bugs Fixed (https://launchpad.net/compiz/+milestone/" + version + ")\n\n"
release_msg += bugs_formatted_msg

print release_msg

# Edit release message
news_update_file = open (".release-script-" + version, "w+")
news_update_file.write (release_msg.encode ("utf8"))
news_update_file.close ()

os.system (editor + " .release-script-" + version)

# Open NEWS
news_file = open ("../NEWS", "r")
news_lines = news_file.readlines ()
news_prepend_file = open (".release-script-" + version, "r")
news_prepend_lines = news_prepend_file.readlines ()

for i in range (0, len (news_prepend_lines)):
    news_prepend_lines[i] = news_prepend_lines[i].decode ("utf8")

news_prepend_lines.append ("\n")

for line in news_lines:
    news_prepend_lines.append (line.decode ("utf8"))

news = ""

for line in news_prepend_lines:
    news += line

news_file.close ()
news_prepend_file.close ()
news_file = open ("../NEWS", "w")
news_file.write (news.encode ("utf8"))
news_file.close ()

# Commit
compiz_tree = bzrlib.workingtree.WorkingTree.open ("..")
compiz_tree.commit ("Release version " + version)

# Create a tag
compiz_branch.unlock ()
compiz_branch.lock_write ()
compiz_branch.tags.set_tag ("v" + version, compiz_branch.last_revision ())
compiz_branch.unlock ()

# Update version
version_file = open ("../VERSION", "w")
version_file.write (sys.argv[1] + "\n")
version_file.close ()

# Commit
compiz_tree = bzrlib.workingtree.WorkingTree.open ("..")
compiz_tree.commit ("Bump VERSION to " + sys.argv[1])
