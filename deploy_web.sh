#!/bin/bash

# XScreenSaver Web Deployment Script
# This script deploys the build_web contents to GitHub Pages

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to show usage
show_usage() {
    echo "Usage: $0 [options]"
    echo ""
    echo "Options:"
    echo "  --help                    - Show this help message"
    echo "  --force                   - Force deployment even if no changes"
    echo "  --build-first             - Build web version before deploying"
    echo "  --message <commit_msg>    - Custom commit message"
    echo ""
    echo "Examples:"
    echo "  $0                        # Deploy current build_web contents"
    echo "  $0 --build-first          # Build then deploy"
    echo "  $0 --force                # Force deployment"
    echo "  $0 --message 'New version' # Deploy with custom commit message"
}

# Parse command line arguments
FORCE_DEPLOY=false
BUILD_FIRST=false
COMMIT_MESSAGE="Deploy XScreenSaver web build to GitHub Pages [ci skip]"

while [[ $# -gt 0 ]]; do
    case $1 in
        --help)
            show_usage
            exit 0
            ;;
        --force)
            FORCE_DEPLOY=true
            shift
            ;;
        --build-first)
            BUILD_FIRST=true
            shift
            ;;
        --message)
            COMMIT_MESSAGE="$2"
            shift 2
            ;;
        *)
            print_error "Unknown option: $1"
            show_usage
            exit 1
            ;;
    esac
done

print_status "🚀 XScreenSaver Web Deployment Script"
print_status "📁 Repository: $(git config --get remote.origin.url)"

# Check if we're in a git repository
if ! git rev-parse --git-dir > /dev/null 2>&1; then
    print_error "Not in a git repository. Please run this script from the project root."
    exit 1
fi

# Check if build_web directory exists
if [ ! -d "build_web" ]; then
    print_error "build_web directory not found!"
    if [ "$BUILD_FIRST" = true ]; then
        print_status "Building web version first..."
        if [ -f "build_web.sh" ]; then
            ./build_web.sh
        else
            print_error "build_web.sh not found. Please build the web version manually."
            exit 1
        fi
    else
        print_error "Please run ./build_web.sh first to create the build_web directory."
        exit 1
    fi
fi

# Check if build_web has the required files
if [ ! -f "build_web/index.html" ] || [ ! -f "build_web/index.js" ] || [ ! -f "build_web/index.wasm" ]; then
    print_error "Required web files not found in build_web directory!"
    print_error "Expected: index.html, index.js, index.wasm"
    if [ "$BUILD_FIRST" = true ]; then
        print_status "Building web version first..."
        if [ -f "build_web.sh" ]; then
            ./build_web.sh
        else
            print_error "build_web.sh not found. Please build the web version manually."
            exit 1
        fi
    else
        print_error "Please run ./build_web.sh first to build the web version."
        exit 1
    fi
fi

print_success "✅ Web build files found in build_web directory"

# Create a temporary directory for deployment
DEPLOY_DIR="gh-pages-tmp"
print_status "🧹 Cleaning temporary deployment directory..."
rm -rf "$DEPLOY_DIR"
mkdir -p "$DEPLOY_DIR"

# Copy everything from build_web directory
print_status "📦 Copying files from build_web..."
cp -r build_web/* "$DEPLOY_DIR/"

# Add a README for GitHub Pages
print_status "📝 Creating README for GitHub Pages..."
cat > "$DEPLOY_DIR/README.md" << 'EOF'
# XScreenSaver Web Demo

A web version of XScreenSaver's HexTrail screensaver, compiled to WebAssembly using Emscripten.

## 🎮 Play Now

**[Click here to run the screensaver!](https://rebroad.github.io/xscreensaver/)**

## 🛠️ Technical Details

- Built with XScreenSaver
- Compiled to WebAssembly using Emscripten
- Uses WebGL2 for hardware-accelerated graphics
- Real-time particle system simulation

## 🎯 Features

- Interactive mouse controls
- Real-time particle trails
- Hardware-accelerated graphics
- Cross-platform compatibility

## 🚀 Local Development

To run locally:
```bash
cd build_web
python3 -m http.server 8000
# Then open http://localhost:8000
```

---

*This is an automatically generated deployment from the main development repository.*
EOF

# Check if gh-pages branch exists
if git show-ref --verify --quiet refs/remotes/origin/gh-pages; then
    print_status "📋 gh-pages branch exists, checking out..."
    # Create or update gh-pages worktree
    git worktree remove gh-pages-deploy 2>/dev/null || true
    git worktree add -B gh-pages gh-pages-deploy origin/gh-pages 2>/dev/null || git worktree add -B gh-pages gh-pages-deploy
else
    print_status "📋 Creating new gh-pages branch..."
    # Create new orphan gh-pages branch
    git worktree remove gh-pages-deploy 2>/dev/null || true
    git worktree add -B gh-pages gh-pages-deploy
fi

# Clean the worktree and copy new files
print_status "🧹 Cleaning gh-pages worktree..."
rm -rf gh-pages-deploy/*

# Copy deployment files
print_status "📦 Copying deployment files..."
cp -r "$DEPLOY_DIR"/* gh-pages-deploy/

# Stage and commit changes
cd gh-pages-deploy

# Check if there are any changes to commit
if git diff --quiet && git diff --cached --quiet; then
    if [ "$FORCE_DEPLOY" = true ]; then
        print_warning "No changes detected, but forcing deployment..."
        # Create an empty commit to force deployment
        git commit --allow-empty -m "$COMMIT_MESSAGE"
    else
        print_status "No changes detected. Use --force to deploy anyway."
        cd ..
        git worktree remove gh-pages-deploy
        rm -rf "$DEPLOY_DIR"
        exit 0
    fi
else
    print_status "📝 Committing changes..."
    git add .
    git commit -m "$COMMIT_MESSAGE"
fi

# Push to GitHub Pages
print_status "🚀 Pushing to GitHub Pages..."
if git push origin gh-pages; then
    print_success "✅ Successfully deployed to GitHub Pages!"
    
    # Get the repository name for the URL
    REPO_URL=$(git config --get remote.origin.url)
    if [[ $REPO_URL == *"github.com"* ]]; then
        # Extract username/repo from git URL
        if [[ $REPO_URL == *"@"* ]]; then
            # SSH format: git@github.com:username/repo.git
            REPO_PATH=$(echo "$REPO_URL" | sed 's/.*github.com[:/]\([^.]*\)\.git/\1/')
        else
            # HTTPS format: https://github.com/username/repo.git
            REPO_PATH=$(echo "$REPO_URL" | sed 's/.*github.com\/\([^.]*\)\.git/\1/')
        fi
        GITHUB_PAGES_URL="https://$REPO_PATH/"
        print_success "🌐 Your site will be available at: $GITHUB_PAGES_URL"
        print_warning "⚠️  It may take a few minutes for changes to appear."
    fi
else
    print_error "❌ Failed to push to GitHub Pages!"
    cd ..
    git worktree remove gh-pages-deploy
    rm -rf "$DEPLOY_DIR"
    exit 1
fi

# Cleanup
cd ..
git worktree remove gh-pages-deploy
rm -rf "$DEPLOY_DIR"

print_success "🎉 Deployment completed successfully!"
print_status "📋 Summary:"
print_status "  - Files deployed: $(ls build_web/ | wc -l) files"
print_status "  - Commit message: $COMMIT_MESSAGE"
print_status "  - Branch: gh-pages" 