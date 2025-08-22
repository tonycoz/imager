This is the Security Policy for the Perl Imager distribution.

Security vulnerabilities can be reported via the [Imager GitHub
repository Security Advisories
page](https://github.com/tonycoz/imager/security/advisories/new).  (If
you do not have access to GitHub, then you can report issues via email
to tony@develop-help.com.)

The latest version of the Security Policy can be found in the
[git repository for Imager](https://github.com/tonycoz/imager).

This text is based on the CPAN Security Group's Guidelines for Adding
a Security Policy to Perl Distributions (version 1.4.2)
https://security.metacpan.org/docs/guides/security-policy-for-authors.html

# How to Report a Security Vulnerability

Security vulnerabilities can be reported via the [Imager GitHub
repository Security Advisories
page](https://github.com/tonycoz/imager/security/advisories/new).  (If
you do not have access to GitHub, then you can report issues via email
to tony@develop-help.com.)

Please include as many details as possible, including code samples or
test cases, so that we can reproduce the issue.  If your issue depends
on image or font files include those as well.

Include the versions of the image and font handling libraries that
Imager was built with. If you are using the libraries packaged for
your OS distribution include that in the report and the OS
distribution they are packaged with.

Check that your report does not expose any sensitive data, such as
passwords, tokens, or personal information.

Project maintainers will normally credit the reporter when a
vulnerability is disclosed or fixed.  If you do not want to be
credited publicly, please indicate that in your report.

If you would like any help with triaging the issue, or if the issue
is being actively exploited, please copy the report to the CPAN
Security Group (CPANSec) at <cpan-security@security.metacpan.org>.

Please *do not* use the public issue reporting system on RT or
GitHub issues for reporting security vulnerabilities.

Please do not disclose the security vulnerability in public forums
until past any proposed date for public disclosure, or it has been
made public by the maintainers or CPANSec.  That includes patches or
pull requests or mitigation advice.

For more information, see
[Report a Security Issue](https://security.metacpan.org/docs/report.html)
on the CPANSec website.

## Response to Reports

The maintainer(s) aim to acknowledge your security report as soon as
possible.  However, this project is maintained by a single volunteer in
their spare time, and they cannot guarantee a rapid response.  If you
have not received a response from them within a week, then
please send a reminder to them and copy the report to CPANSec at
<cpan-security@security.metacpan.org>.

Please note that the initial response to your report will be an
acknowledgement, with a possible query for more information.  It
will not necessarily include any fixes for the issue.

The project maintainer(s) may forward this issue to the security
contacts for other projects where we believe it is relevant.  This
may include embedded libraries, system libraries, prerequisite
modules or downstream software that uses this software.

They may also forward this issue to CPANSec.

# Which Software This Policy Applies To

Any security vulnerabilities in Imager are covered by this policy.

Security vulnerabilities in versions of any libraries that are
included in Imager are also covered by this policy.

Security vulnerabilities are considered anything that allows users
to execute unauthorised code, access unauthorised resources, or to
have an adverse impact on accessibility, integrity or performance of a system.

Security vulnerabilities in upstream software (prerequisite modules
or system libraries, or in Perl), are not covered by this policy.

Security vulnerabilities in downstream software (any software that
uses Imager, or plugins to it that are not included with the
Imager distribution) are not covered by this policy.

## Supported Versions of Imager

The maintainer(s) will release security fixes for the latest version
of Imager.

The maintainer may provide patches or advice for fixes to older
versions of Imager shipped with your OS distribution if requested by
the OS distribution package maintainers.

# Installation and Usage Issues

The distribution metadata specifies minimum versions of
prerequisites that are required for Imager to work.  However, some
of these prerequisites may have security vulnerabilities, and you
should ensure that you are using up-to-date versions of these
prerequisites.

Where security vulnerabilities are known, the metadata may indicate
newer versions as recommended.

Imager will use image or font format support libraries based on
compatibility, security issues in those libraries should be dealt with
by updating those to their fixed versions.

## Usage

Please see the software documentation for further information.
