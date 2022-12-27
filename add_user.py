import json
from argparse import ArgumentParser, Namespace
from pathlib import Path
from typing import List, Dict

def main(args):
    # print(args.new_user)
    if args.new_user:
        users = []
        with open('users.json','r') as file:
            text = file.read()
            if text:
                #print(text)
                users = json.loads(text)
        new_user = json.loads(args.new_user)
        new_user["sessionId"]=""
        users.append(new_user)
        new_json = json.dumps(users, indent=4)
        with open('users.json','w') as file:
            file.write(new_json)

def parse_args() -> Namespace:
    parser = ArgumentParser()
    parser.add_argument(
        "--new_user",
        type=str,
    )
    args = parser.parse_args()
    return args

if __name__ == "__main__":
    args = parse_args()
    main(args)