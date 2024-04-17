<!--
SPDX-FileCopyrightText: Huawei Inc.

SPDX-License-Identifier: CC-BY-4.0
-->

-   [Gitlab Contributions](#gitlab-contributions)
    -   [Overview](#overview)
    -   [Commit Guidelines](#commit-guidelines)
    -   [Contributions to Documentation](#contributions-to-documentation)
-   [REUSE Compliance](#reuse-compliance)
    -   [SPDX Information and REUSE Standard](#spdx-information-and-reuse-standard)
        -   [SPDX Header Example](#spdx-header-example)
        -   [Substantial Contributions](#substantial-contributions)
-   [DCO sign-off](#dco-sign-off)
    -   [Overview](#overview-1)
    -   [Developer Certificate of Origin](#developer-certificate-of-origin)
-   [`oniro`-specific contributions process and guidelines](#oniro-specific-contributions-process-and-guidelines)

# Gitlab Contributions

## Overview

Oniro Project handles contributions as [merge requests](https://docs.gitlab.com/ee/user/project/merge_requests/) to relevant repositories part of the Oniro Project [GitLab instance](https://gitlab.eclipse.org/eclipse/oniro-core). The flow for handling that is classic: fork-based merge requests. This means that once you have an account, you can fork any repository, create a branch with proposed changes and raise a merge request against the forked repository. More generic information you can find on the Gitlab's documentation as part of ["Merge requests workflow"](https://docs.gitlab.com/ee/development/contributing/merge_request_workflow.html).

## Commit Guidelines

<div class="note">

<div class="title">

Note

</div>

If you are new to `git`, start by reading the official [Getting Started Document](https://git-scm.com/book/en/v2/Getting-Started-First-Time-Git-Setup).

</div>

At its core, contributing to the Oniro Project project means *wrapping* your work as `git` commits. How we handle this has an impact on rebasing, cherry-picking, back-porting, and ultimately exposing consistent documentation through its logs.

To achieve this, we maintain the following commit guidelines:

-   Each commit should be able to stand by itself providing a building block as part of the MR.
    -   A good balance of granularity with scoped commits helps to handle backports (e.g. cherry-picks) and also improves the ability to review smaller chunks of code taking commit by commit.
-   Changes that were added on top of changes introduced in the MR, should be squashed into the initial commit.
    -   For example, a MR that introduced a new build system recipe and, as a separate commit, fixed a build error in the initial recipe. The latter commit should be squashed into the initial commit.
    -   For example, a MR introducing a new docs chapter and also adding, as a separate commit, some typo fixes. The latter commits should be squashed into the initial commit.
    -   There is a small set of exceptions to this rule. All these exceptions gravitate around the case where an MR, even if it provides multiple commits in the same scope (for example, to the same build recipe), each of the commits has a very specific purpose.
        -   For example, a line formating change followed by a chapter addition change in the same documentation file.
        -   Also, it can be the case of two functional changes that are building blocks in the same scope.
        -   Another example where commits are not to be squashed is when having a commit moving the code and a commit modifying the code in the new location.
-   Make sure you clean your code of trailing white spaces/tabs and that each file ends with a new line.
-   Avoid *merge* commits as part of your MR. Your commits should be rebased on top of the *HEAD* of the destination branch.

As mentioned above, *git log* becomes informally part of the documentation of the product. Maintaining consistency in its format and content improves debugging, auditing, and general code browsing. To achieve this, we also require the following commit message guidelines:

-   The *subject* line (the first line) needs to have the following format: `scope: Title limited to 80 characters`.
    -   Use the imperative mood in the *subject* line for the *title*.
    -   The *scope* prefix (including the colon and the following whitespace) is optional but most of the time highly recommended. For example, fixing an issue for a specific build recipe, would use the recipe name as the *scope*.
    -   The *title* (the part after the *scope*) starts with a capital letter.
    -   The entire *subject* line shouldn't exceed 80 characters (same text wrapping rule for the commit body).
-   The commit *body* separated by an empty line from the *subject* line.
    -   The commit *body* is optional but highly recommended. Provide a clear, descriptive text block that accounts for all the changes introduced by a specific commit.
    -   The commit *body* must not contain more than 80 characters per line.
-   The commit message will have the commit message *trailers* separated by a new line from the *body*.
    -   Each commit requires at least a *Signed-off-by* trailer line. See more as part of the `/contributing/dco` document.
    -   All *trailer* lines are to be provided as part of the same text block - no empty lines in between the *trailers*.

Additional commit message notes:

-   Avoid using special characters anywhere in the commit message.
-   Be succinct but descriptive.
-   Have at least one *trailer* as part of each commit: *Signed-off-by*.
-   You can automatically let `git` add the *Signed-off-by* by taking advantage of its `-s` argument.
-   Whenever in doubt, check the existing log on the file (`<FILE>`) you are about to commit changes, using something similar to: `git log <FILE>`.

Example of a full git message:

``` text
busybox: Add missing dependency on virtual/crypt

Since version 1.29.2, the busybox package requires virtual/crypt. Add this
to DEPENDS to make sure the build dependency is satisfied.

Signed-off-by: Joe Developer <joe.developer@example.com>
```

## Contributions to Documentation

In Oniro Project, the documentation usually stays with the respective code repositories. This means that contributing to documentation is not in any way different than contributing to code. The processes, contribution guidelines are to remain the same. The only difference is that documentation files are to be released under `Creative Commons License version 4.0`.

Documentation that doesn't link directly to one specific repository, is available in the [docs repository](https://gitlab.eclipse.org/eclipse/oniro-core/docs).

In terms of file format, the project unifies its documentation as `ReStructuredText` files. A RestructuredText primer is available as part of the Sphinx [documentation](https://www.sphinx-doc.org/en/master/usage/restructuredtext/basics.html).

As a rule of thumb, anything that ends up compiled in the project documentation is to maintain the RestructuredText file format. Text files that are not meant to be compiled as part of the project's documentation can be written in [Markdown](https://daringfireball.net/projects/markdown/). For example, a repository `README` file can be written in Markdown as it doesn't end up compiled in the project-wide documentation.

# REUSE Compliance

## SPDX Information and REUSE Standard

All projects and files for an hosted project **MUST** be [REUSE](https://reuse.software/) compliant. REUSE requires SPDX information for each file, rules for which are as follows:

-   Any new file must have a SPDX header (copyright and license).
-   For files that don't support headers (for example binaries, patches etc.) an associated `.license` file must be included with the relevant SPDX information.
-   Do not add Copyright Year as part of the SPDX header information.
-   The general rule of thumb for the license of a patch file is to use the license of the component for which the patch applies.
-   When modifying a file through this contribution process, you may (but don't have to) claim copyright by adding a copyright line.
-   Never alter copyright statements made by others, but only add your own.

Some files will make an exception to the above rules as described below:

-   Files for which copyright is not claimed and for which this information was not trivial to fetch (for example backporting patches, importing build recipes etc. when upstream doesn't provide the SPDX information in the first place)
-   license files (for example `common-licenses` in bitbake layers)

### SPDX Header Example

Make sure all of your submitted new files have a licensing statement in the headers. Please make sure that the license for your file is consistent with the licensing choice at project level and that you select the correct SPDX identifier, as in the following example for Apache 2.0 license:

``` text
/*
 * SPDX-FileCopyrightText: Jane Doe <jane@example.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */
```

### Substantial Contributions

Therefore, if your contribution is only a patch directly applied to an existing file, then you are not required to do anything. If your contribution is an entire new project, or a substantial, copyrighted contribution, you **MUST** make sure that you do that following the [IP Policy](https://booting.oniroproject.org/distro/governance/ip-policy) and that you comply with REUSE standard to include the licensing information where they are required.

# DCO sign-off

## Overview

Commits **MUST** be submitted only with a sign-off by the submitter. A commit without a sign-off will be automatically rejected. You don't need be the author or the copyright holder of the contribution, but you must make sure that you have the power to submit on behalf of those who are.

To sign your work, just add a line like this at the end of your commit message:

``` text
Signed-off-by: Jane Doe <jane@example.com>
```

This could be done automatically in the `git` submission:

``` text
git commit --signoff -m "comment"
```

## Developer Certificate of Origin

By doing this you state that you certify the following (from [https://developercertificate.org](https://developercertificate.org/)):

``` text
Developer Certificate of Origin
Version 1.1

Copyright (C) 2004, 2006 The Linux Foundation and its contributors.
1 Letterman Drive
Suite D4700
San Francisco, CA, 94129

Everyone is permitted to copy and distribute verbatim copies of this
license document, but changing it is not allowed.


Developer's Certificate of Origin 1.1

By making a contribution to this project, I certify that:

(a) The contribution was created in whole or in part by me and I
    have the right to submit it under the open source license
    indicated in the file; or

(b) The contribution is based upon previous work that, to the best
    of my knowledge, is covered under an appropriate open source
    license and I have the right under that license to submit that
    work with modifications, whether created in whole or in part
    by me, under the same open source license (unless I am
    permitted to submit under a different license), as indicated
    in the file; or

(c) The contribution was provided directly to me by some other
    person who certified (a), (b) or (c) and I have not modified
    it.

(d) I understand and agree that this project and the contribution
    are public and that a record of the contribution (including all
    personal information I submit with it, including my sign-off) is
    maintained indefinitely and may be redistributed consistent with
    this project or the open source license(s) involved.
```

# `oniro`-specific contributions process and guidelines

The project handles contributions as merge request in the project's
[GitLab instance](https://git.ostc-eu.org/distro/oniro). See above for
more details.

Each contributions must adhere to the [OpenEmbedded Commit Patch Message Guidelines](http://www.openembedded.org/wiki/Commit_Patch_Message_Guidelines).
This includes (but not limited to) having a well-formatted `git log` subject.
Whenever you are in doubt of the subject line format, run `git log
./<filename>` for examples.

The general format is:

```
component: Subject line

Optional description.

Signed-off-by: Joe Developer <joe.developer@example.com>
```

Please note that by signing off the commit, you make a **legally binding
statement** that you certify as in the [DCO sign-off](#dco-sign-off) section.
