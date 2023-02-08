# TrustEVM Contract CI
This GitHub Actions workflow builds the TrustEVM contract and its associated tests.

### Index
1. [Triggers](#triggers)
1. [Inputs](#inputs)
1. [Steps](#steps)
1. [Outputs](#outputs)
1. [GitHub App Integration](#github-app-integration)
1. [See Also](#see-also)

## Triggers
This GitHub action will run under the following circumstances:
1. When code is pushed to the `main` branch.
1. When code is pushed to any branch with a name starting with `release/`.
1. Workflow dispatch event, which is triggered manually using the "Workflow Dispatch" button in the Actions tab of the GitHub repository.

## Inputs
The inputs for this GitHub action are:
1. `DCMAKE_BUILD_TYPE` - defined in the GitHub Action YAML, this sets the build type and determines the level of optimization, debugging information, and other flags; one of `Debug`, `Release`, `RelWithDebInfo`, or `MinSizeRel`.
1. `DWITH_TEST_ACTIONS` - defined in the GitHub Action YAML, build with or without code paths intended to be excercised exclusively by tests.
1. `GITHUB_TOKEN` - a GitHub Actions intrinsic used to access the repository and other public resources.
1. `TRUSTEVM_CI_APP_ID` - the app ID of the `trustevm-ci-submodule-checkout` GitHub App.
1. `TRUSTEVM_CI_APP_KEY` - the private key to the `trustevm-ci-submodule-checkout` GitHub App.

These inputs are used in various steps of the workflow to perform actions such as authentication, downloading artifacts, configuring the build, and uploading artifacts.

## Steps
This workflow performs the following steps:
1. Authenticate to the `trustevm-ci-submodule-checkout` GitHub app using the [AntelopeIO/github-app-token-action](https://github.com/AntelopeIO/github-app-token-action) action to obtain an ephemeral token.
1. Checkout the repo and submodules using the ephemeral token.
1. Attach an annotation to the build with CI documentation.
1. Download the CDT binary using the [AntelopeIO/asset-artifact-download-action](https://github.com/AntelopeIO/asset-artifact-download-action) action.
1. Install the CDT binary.
1. Build the TrustEVM contract using `make` and `cmake`.
1. Upload the contract build folder to GitHub Actions.
1. Download the `leap-dev` binary using [AntelopeIO/asset-artifact-download-action](https://github.com/AntelopeIO/asset-artifact-download-action) action.
1. Install the `leap-dev` binary.
1. Build the TrustEVM contract tests using `make` and `cmake`.
1. Upload the build folder for the contract test code to GitHub Actions.

## Outputs
This workflow produces the following outputs:
1. Contract Build Artifacts - `contract.tar.gz` containing the built contract from the `contract/build` folder.
1. Contract Test Artifacts - `contract-test.tar.gz` containing the built contract test artifacts from the `contract/tests/build` folder.

Note that, due to actions/upload-artifact [issue 39](https://github.com/actions/upload-artifact/issues/39) which has been open for over _three years_ and counting, the archives attached as artifacts will be zipped by GitHub when you download them such that you get a `*.zip` containing the `*.tar.gz`. There is nothing anyone can do about this except for GitHub.

## GitHub App Integration
This workflow uses the [AntelopeIO/github-app-token-action](https://github.com/AntelopeIO/github-app-token-action) GitHub action to assume the role of a GitHub application installed to the AntelopeIO organization to clone the private submodules. It requests a token from the GitHub app, clones everything using this token under the identity of the app, then the token expires. This is advantageous over a persistent API key from a GitHub service account because this does not consume a paid user seat, the "account" associated with the app cannot be logged into in the GitHub web UI, the app is scoped to exactly the permissions it needs to perform the clones for this repo _and nothing more_, and the API key expires very quickly so a bad actor who exfiltrates this key from the CI system should find it is not useful.

**The downside is that if TrustEVM adds additional private submodules, the GitHub app must be granted permissions to these new submodules.** The CI system will not work until this happens.

## See Also
- [asset-artifact-download-action](https://github.com/AntelopeIO/asset-artifact-download-action) GitHub Action
- [github-app-token-action](https://github.com/AntelopeIO/github-app-token-action) GitHub action
- [TrustEVM Documentation](../../README.md)

For assistance with the CI system, please open an issue in this repo or reach out in the `#help-automation` channel via IM.

***
**_Legal notice_**  
This document was generated in collaboration with ChatGPT from OpenAI, a machine learning algorithm or weak artificial intelligence (AI). At the time of this writing, the [OpenAI terms of service agreement](https://openai.com/terms) §3.a states:
> Your Content. You may provide input to the Services (“Input”), and receive output generated and returned by the Services based on the Input (“Output”). Input and Output are collectively “Content.” As between the parties and to the extent permitted by applicable law, you own all Input, and subject to your compliance with these Terms, OpenAI hereby assigns to you all its right, title and interest in and to Output.

This notice is required in some countries.
