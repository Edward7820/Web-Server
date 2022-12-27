import json
from argparse import ArgumentParser, Namespace
from pathlib import Path
from typing import List, Dict

def main(args):
    # print(args.login_user,args.sessionId)
    login = False
    if args.login_user:
        users = []
        with open('users.json','r') as file:
            text = file.read()
            if text:
                #print(text)
                users = json.loads(text)
        login_user = json.loads(args.login_user)
        for user in users:
            if user['username']==login_user['username'] and user['password']==login_user['password']:
                # log in successfully
                user['sessionId']=args.sessionId
                login = True
                break

        if login:
            new_json = json.dumps(users, indent=4)
            with open('users.json','w') as file:
                file.write(new_json)
            exit(0)
        else:
            exit(1)
            
def parse_args() -> Namespace:
    parser = ArgumentParser()
    parser.add_argument(
        "--login_user",
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