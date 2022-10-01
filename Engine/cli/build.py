import requests
import os
import json
import datetime
import time
import sys

def write_json(path, content):
    with open(path, "w") as file: 
        file.write(json.dumps(content))

def needs_update(cache_contents, filename, newest_version):
    if filename in cache_contents:
        return newest_version != cache_contents[filename]['updated_at']
    else:
        return True
    
def download_item(dest_path, url):
    r = requests.get(url, stream=True)
    with open(dest_path, 'wb') as fd:
        for chunk in r.iter_content(chunk_size=128):
            fd.write(chunk)

def update_cache_desc(cache_path, filename, updated_at):
    with open(cache_path, "r") as file: 
        json_content = json.loads(file.read())
    if filename in json_content.keys():
        json_content[filename]['updated_at'] = updated_at
    else:
        json_content[filename] = {'updated_at': updated_at}
    write_json(cache_path, json_content)    


def download_gists(root_dir, download_list):

    dirpath = os.path.join(root_dir, "gist", "github")
    dirpath2 = os.path.join(root_dir, "gist")
    cache_desc_path = os.path.join(dirpath, "desc.json")
    
    if not os.path.isdir(dirpath2):
        os.mkdir(dirpath2)
    if not os.path.isdir(dirpath):
        os.mkdir(dirpath)    
    if not os.path.exists(cache_desc_path):
        write_json(cache_desc_path, {})

    mapping = {}

    r = False
    try:
        headers = {'Accept': 'application/vnd.github.v3+json'}
        r = requests.get("https://api.github.com/users/BluBloos/gists", headers)
    except:
        print('Error w/ gists retrieval. Maybe network is down?')
        exit()

    for elem in r.json():
        git_pull_url = elem['git_pull_url']
        arr = [key for key in elem['files'].keys()]
        filename = arr[0]
        raw_url = elem['files'][filename]['raw_url']
        mapping[filename] = (raw_url, elem['updated_at'])

    with open(cache_desc_path, "r") as file:
        cache_contents = json.loads(file.read())

    for item in download_list:
        if item not in mapping.keys():
            print(item, "not in mapping")
        else:
            raw_url, updated_at = mapping[item]
            if needs_update(cache_contents, item, updated_at):
                # do the download
                print("downloading", item, "to", dirpath)
                download_item(os.path.join(dirpath, item), raw_url)
                update_cache_desc(cache_desc_path, item, updated_at)
            else:
                print("no update needed for", item)

# given a filename, return a list of the gists from this file.
def get_gists_from_file(fileName):
    gists = []
    with open(fileName, "r") as file:
        lines = file.readlines()
    for line in lines:
        line = line.strip() # get rid of any whitespace.
        # NOTE(Noah): We only have it be the case that we parse the include
        # statements with the angle brackets.
        if line.startswith("#include <gist/github/"):
            gistName = line[22:-1]
            gists.append(gistName)
    return gists

def get_gists(root_dir):

    # construct the file paths that need opening.
    file_paths = []
    dirs = [os.path.join(root_dir, "src"), os.path.join(root_dir, "include")]
    print("scanning source in dirs={}".format(dirs))

    blacklist = []

    while ( len(dirs) > 0 ):
        dir = dirs.pop(0) # pop of stack.
        for file in os.listdir(dir):
            file_path = os.path.join(dir, file)
            if not os.path.isdir(file_path):
                file_paths.append(file_path)
            elif file_path not in blacklist:
                dirs.append(file_path)

    download_list = []
    for path in file_paths:
        for gist in get_gists_from_file(path):
            if gist not in download_list:
                download_list.append(gist)

    print("generated download list of {}".format(download_list))

    download_gists(root_dir, download_list)

if __name__ == "__main__":
    print("\n------\nRunning Automata Engine gisthook")
    get_gists(sys.argv[1])
    print("------\n")