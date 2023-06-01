pluginManagement {
    repositories {
        maven ("https://maven.aliyun.com/repository/central")
        maven ("https://maven.aliyun.com/repository/jcenter")
        maven ("https://maven.aliyun.com/repository/google")
        maven ("https://maven.aliyun.com/repository/public")
    }
}
dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        maven ("https://maven.aliyun.com/repository/central")
        maven ("https://maven.aliyun.com/repository/jcenter")
        maven ("https://maven.aliyun.com/repository/google")
        maven ("https://maven.aliyun.com/repository/public")
    }
}

rootProject.name = "Streaming"
include(":app")
include(":libstreaming")
 