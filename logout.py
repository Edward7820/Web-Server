import json
from argparse import ArgumentParser, Namespace
from pathlib import Path
from typing import List, Dict

def main(args):
    # print(args.login_user,args.sessionId)
    if args.sessionId:
        users = []
        with open('users.json','r') as file:
            text = file.read()
            if text:
                users = json.loads(text)
        for user in users:
            if user['sessionId']==args.sessionId:
                user['sessionId']=""

        new_json = json.dumps(users, indent=4)
        with open('users.json','w') as file:
            file.write(new_json)
            
def parse_args() -> Namespace:
    parser = ArgumentParser()
    parser.add_argument(
        "--sessionId",
        type=str,
    )
    args = parser.parse_args()
    return args

if __name__ == "__main__":
    args = parse_args()
    main(args)