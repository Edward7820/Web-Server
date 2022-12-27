import json
from argparse import ArgumentParser, Namespace
from pathlib import Path
from typing import List, Dict

def main(args):
    username = ""
    if args.sessionId:
        users = []
        with open('users.json','r') as file:
            text = file.read()
            if text:
                users = json.loads(text)
        for user in users:
            if user['sessionId']==args.sessionId:
                username = user['username']
                break
        
    if args.new_message and username:
        messages = []
        with open('messages.json','r') as file:
            text = file.read()
            if text:
                #print(text)
                messages = json.loads(text)
        messages.append({"username":username, "message":args.new_message})
        new_json = json.dumps(messages, indent=4)
        with open('messages.json','w') as file:
            file.write(new_json)

def parse_args() -> Namespace:
    parser = ArgumentParser()
    parser.add_argument(
        "--new_message",
        type=str,
    )
    parser.add_argument(
        "--sessionId",
        type=str,
    )
    args = parser.parse_args()
    return args

if __name__ == "__main__":
    args = parse_args()
    main(args)