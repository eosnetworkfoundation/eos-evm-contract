# TrustEVM Node CI
This GitHub Actions workflow builds the TrustEVM node.

### Index
1. [Triggers](#triggers)
1. [Inputs](#inputs)
1. [Steps](#steps)
1. [Outputs](#outputs)
1. [See Also](#see-also)

## Triggers
This GitHub action will run under the following circumstances:
1. When code is pushed.
1. Workflow dispatch event, a manual CI run, which can be triggered by the "Workflow Dispatch" button in the Actions tab of the GitHub repository, among other means.

## Inputs
The inputs for this GitHub action are:
1. `GITHUB_TOKEN` - a GitHub Actions intrinsic used to access the repository and other public resources.
1. `TRUSTEVM_CI_APP_ID` - the app ID of the `trustevm-ci-submodule-checkout` GitHub App.
1. `TRUSTEVM_CI_APP_KEY` - the private key to the `trustevm-ci-submodule-checkout` GitHub App.
1. `upload-artifacts` - a boolean input that specifies whether or not to upload the artifacts of the build. The default value is `false`. This can be overridden in manual CI runs.

These inputs are used in various steps of the workflow to perform actions such as authentication, downloading artifacts, configuring the build, and uploading artifacts.

## Steps
This workflow performs the following steps:
1. Attach Documentation - GitHub runner
    1. Checkout the repo with no submodules.
    1. Attach an annotation to the GitHub Actions build summary page containing CI documentation.
1. TrustEVM Node Build - ENF runner
    1. Authenticate to the `trustevm-ci-submodule-checkout` GitHub app using the [AntelopeIO/github-app-token-action](https://github.com/AntelopeIO/github-app-token-action) action to obtain an ephemeral token.
    1. Checkout the repo and submodules using the ephemeral token.
    1. Install dependencies, then build the TrustEVM node using `cmake` and `make`. - Ubuntu 20.04 docker container
    1. Upload the node build folder to GitHub Actions if the `upload-artifacts` input is set to `true`.

## Outputs
This workflow produces the following outputs:
1. Build Artifacts - `build.tar.gz` containing the built artifacts of TrustEVM Node, if the `upload-artifacts` input is set to `true`.

> 💾️ Build artifacts are only attached on-demand for this pipeline because they are >117 MB each, but we only get 2 GB of cumulative artifact storage in GitHub Actions while TrustEVM is a private repo.

## See Also
- [github-app-token-action](https://github.com/AntelopeIO/github-app-token-action) GitHub action
- [TrustEVM Documentation](../../README.md)

For assistance with the CI system, please open an issue in this repo or reach out in the `#help-automation` channel via IM.

***
**_Legal notice_**  
This document was generated in collaboration with ChatGPT from OpenAI, a machine learning algorithm or weak artificial intelligence (AI). At the time of this writing, the [OpenAI terms of service agreement](https://openai.com/terms) §3.a states:
> Your Content. You may provide input to the Services (“Input”), and receive output generated and returned by the Services based on the Input (“Output”). Input and Output are collectively “Content.” As between the parties and to the extent permitted by applicable law, you own all Input, and subject to your compliance with these Terms, OpenAI hereby assigns to you all its right, title and interest in and to Output.

This notice is required in some countries.
