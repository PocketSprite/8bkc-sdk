# based on http://protips.readthedocs.io/link-roles.html

import re
from docutils import nodes
from repo_util import run_cmd_get_output

def get_github_rev():
    path = run_cmd_get_output('git rev-parse --short HEAD')
    tag = run_cmd_get_output('git describe --exact-match')
    print ('Git commit ID: ', path)
    if len(tag):
        print ('Git tag: ', tag)
        path = tag
    return path


def setup(app):
    baseurl = 'https://github.com/PocketSprite/8bkc-sdk'
    rev = get_github_rev()
#    app.add_role('component', autolink('{}/tree/{}/components/%s'.format(baseurl, rev)))

def autolink(pattern):
    def role(name, rawtext, text, lineno, inliner, options={}, content=[]):
        m = re.search('(.*)\s*<(.*)>', text)
        if m:
            link_text = m.group(1)
            link = m.group(2)
        else:
            link_text = text
            link = text
        url = pattern % (link,)
        node = nodes.reference(rawtext, link_text, refuri=url, **options)
        return [node], []
    return role
