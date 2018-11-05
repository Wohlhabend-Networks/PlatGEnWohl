export IS_PULL_REQUEST=false
if [[ "${TRAVIS_PULL_REQUEST}" == "true" ]]; then
    export IS_PULL_REQUEST=true
fi

if [[ "${TRAVIS_BRANCH}" != "" ]]; then
    export GIT_BRANCH=${TRAVIS_BRANCH}
else
    export GIT_BRANCH=$(git branch | sed -n -e 's/^\* \(.*\)/\1/p')
fi
