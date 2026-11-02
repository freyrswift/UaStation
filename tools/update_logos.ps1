param([string]$Renderer = "$PSScriptRoot\..\build\ua_logo_assets.exe")
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$logos = Join-Path $root 'resources\logos'
$checked = Get-Date -Format 'yyyy-MM-dd'
$sources = @(
    @{ id='claude'; url='https://claude.com/icon.svg' },
    @{ id='deepseek'; url='https://www.deepseek.com/favicon.ico' },
    @{ id='codex'; installed='OpenAI.Codex' },
    @{ id='cursor'; url='https://cursor.com/marketing-static/favicon-light.svg' },
    @{ id='windsurf'; url='https://devin.ai/favicon.svg' },
    @{ id='zcode'; url='https://zcode.z.ai/favicon-192x192.png?v=20260707-transparent' },
    @{ id='opencode'; url='https://opencode.ai/favicon-96x96-v3.png' },
    @{ id='workbuddy'; url='https://download.codebuddy.ai/web/workbuddy/170b416d2eccaaf590ea40cd918d5a6b4f07178f/assets/logo.svg' },
    @{ id='gemini'; url='https://geminicli.com/icon.png' },
    @{ id='qwen'; url='https://qwenlm.github.io/qwen-code-docs/favicon.png?v=2' },
    @{ id='trae'; url='https://lf-static.traecdn.us/obj/trae-ai-tx/trae_website/favicon.png' },
    @{ id='codebuddy'; url='https://download.codebuddy.cn/web/website/ef3f61071769c4eb251eaf348beccaa512218f3d/assets/logo.svg' },
    @{ id='cline'; url='https://cline.bot/assets/branding/favicons/favicon-256x256.png' },
    @{ id='copilot'; url='https://raw.githubusercontent.com/primer/octicons/main/icons/copilot-24.svg' },
    @{ id='grok'; url='https://grok.com/images/android-chrome-192x192.png' },
    @{ id='doubao'; url='https://lf-flow-web-cdn.doubao.com/obj/flow-doubao/favicon/new-doubao/192x192.png' },
    @{ id='minimax'; url='https://www.minimax.io/favicon.ico' },
    @{ id='kimi'; url='https://www.kimi.com/pwa-192.png' }
)
$temporary = Join-Path ([IO.Path]::GetTempPath()) ('ua-logos-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $temporary | Out-Null
$manifest = @()
try {
    foreach ($source in $sources) {
        $inputPath = Join-Path $temporary ($source.id + '.asset')
        if ($source.installed) {
            $assetName = 'Square44x44Logo.targetsize-256_altform-lightunplated.png'
            $packageRoots = @()
            $client = Get-Process -Name ChatGPT -ErrorAction SilentlyContinue |
                Where-Object { $_.Path -like '*\OpenAI.Codex_*\app\ChatGPT.exe' } |
                Select-Object -First 1
            if ($client) {
                $packageRoots += Split-Path (Split-Path $client.Path -Parent) -Parent
            }
            $packageRoots += Get-ChildItem -LiteralPath (Join-Path $env:ProgramFiles 'WindowsApps') -Directory -Filter 'OpenAI.Codex_*' -ErrorAction SilentlyContinue |
                Sort-Object { [version]($_.Name -split '_')[1] } -Descending |
                Select-Object -ExpandProperty FullName
            $asset = $packageRoots | Select-Object -Unique |
                ForEach-Object { Join-Path $_ ('assets\' + $assetName) } |
                Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } |
                Select-Object -First 1
            if (-not $asset) { throw 'Install the official Codex desktop client to obtain its package logo.' }
            Copy-Item -LiteralPath $asset -Destination $inputPath
            $package = Split-Path (Split-Path (Split-Path $asset -Parent) -Parent) -Leaf
            $origin = 'Installed official desktop package: ' + $package + '/assets/' + $assetName
        } else {
            Invoke-WebRequest -Uri $source.url -OutFile $inputPath -TimeoutSec 30
            $origin = $source.url
        }
        $output = Join-Path $logos ($source.id + '-color.png')
        & $Renderer $inputPath $output
        if ($LASTEXITCODE -ne 0) { throw "Logo rendering failed: $($source.id), exit $LASTEXITCODE" }
        $manifest += [ordered]@{ id=$source.id; source=$origin; checked=$checked; sha256=(Get-FileHash -LiteralPath $output -Algorithm SHA256).Hash.ToLowerInvariant() }
        Write-Host "Updated $($source.id)"
    }
    $manifest | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $logos 'sources.json') -Encoding utf8
} finally {
    $resolved = [IO.Path]::GetFullPath($temporary)
    if ($resolved.StartsWith([IO.Path]::GetFullPath([IO.Path]::GetTempPath()), [StringComparison]::OrdinalIgnoreCase) -and
        ([IO.Path]::GetFileName($resolved) -like 'ua-logos-*')) {
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
}
