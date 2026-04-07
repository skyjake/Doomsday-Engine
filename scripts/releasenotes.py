#!/usr/bin/env python3
import sys, csv, re, string

issues = {}
features = []
bugs = []
categories = {}
tags = {}

def issue_priority(text):
    return {'Urgent': 5, 'High': 4, 'Normal': 3, 'Low': 2, 'Lowest': 1}[text]
    
def spaces_omitted(text):
    out = ''
    for c in text: 
        if c not in string.whitespace: 
            out += c
    return out

for csv_line in csv.reader(open(sys.argv[1])):
    if csv_line[0] == '#': continue # Header    
    num = int(csv_line[0])
    m = re.match('\[(.*)\](.*)', csv_line[3])
    extra_tag = ''
    if m:
        csv_line[3] = m.group(2).strip()
        extra_tag = m.group(1).strip()
    categ = csv_line[4]
    if csv_line[1] == 'Feature':
        features.append(num)
    elif csv_line[1] == 'Bug':
        bugs.append(num)
    if categ not in categories: categories[categ] = []
    categories[categ].append(num)
    issue_tags = []
    for tag in csv_line[5].split(',') + [extra_tag]:
        tag = tag.strip()
        if len(tag) == 0: continue
        if tag not in tags: tags[tag] = []
        tags[tag].append(num)
        if tag not in issue_tags and \
                tag.lower()[:-1] not in spaces_omitted(csv_line[3]).lower(): 
            issue_tags.append(tag)
    csv_line[5] = issue_tags
    issues[num] = csv_line[1:]        
     
print('== Features ==')

def get_features_in_category(categ):
    feats = []
    for num in categories[categ]:
        if issues[num][0] == 'Feature':
            feats.append(num)
    return feats
    
def issue_tags_list(num):
    return '%s' % ', '.join(issues[num][4])    
    
def issue_meta(num):
    return '<span class="release-notes-meta"> — ' + issue_tags_list(num) + ' {{issue|%i}}</span>' % num

for categ in sorted(categories.keys()):
    featlist = sorted(get_features_in_category(categ), 
                      key=lambda num: issues[num][2])
    if (len(featlist) == 0): continue
    print('\n===', categ, '===')
    for num in featlist:
        issue = issues[num]
        print('*', issue[2], issue_meta(num))

print('\n== Fixed bugs ==')

buglist = sorted(bugs, key=lambda num: -issue_priority(issues[num][1]))
for num in buglist:
    issue = issues[num]
    title = issue[2]
    if issue[3] != 'Defect': title = "%s: %s" % (issue[3], issue[2])
    print('*', title, issue_meta(num))

print('\n== See also ==')
print('\n[[Category:Doomsday releases]]')