#!/bin/bash
set -e # Exit with nonzero exit code if anything fails

# Add rsa keys to the ssh agent to push to GitHub
gpg --output ../id_maestro_rsa --batch --passphrase $DECRYPT_GITHUB_AUTH --decrypt $TRAVIS_BUILD_DIR/id_maestro_rsa.enc
chmod 600 ../id_maestro_rsa
eval `ssh-agent -s`
ssh-add ../id_maestro_rsa
#ls ../id_rsa_travis

cd $TRAVIS_BUILD_DIR
git config remote.origin.url git@github.com:${TRAVIS_PULL_REQUEST_SLUG}.git
git config --get remote.origin.fetch
git config remote.origin.fetch "+refs/heads/*:refs/remotes/origin/*"
git config --get remote.origin.fetch
git fetch origin
git remote -v
git branch --remote
git checkout --track origin/$TRAVIS_PULL_REQUEST_BRANCH

echo "Running tab exterminator script"

cd $TRAVIS_BUILD_DIR/Util/scripts
./tab_exterminator.sh

# Now let's go have some fun with the cloned repo
cd $TRAVIS_BUILD_DIR
git config user.name "Travis CI"
git config user.email "$COMMIT_AUTHOR_EMAIL"

echo "doing git add/commit/push"

# Commit the "changes", i.e. the new version.
# The delta will show diffs between new and old versions.
git add --all

# Exit if there are no docs changes
if git diff --staged --quiet; then
   echo "exiting with no tabs exterminated"
   exit 0
fi

# Otherwise, commit and push
git commit -m "Tabs have been converted to spaces by tab_exterminator.sh"
echo "pushing to $TRAVIS_PULL_REQUEST_BRANCH"
git push origin $TRAVIS_PULL_REQUEST_BRANCH
cd 

# Kill the ssh-agent
ssh-agent -k
