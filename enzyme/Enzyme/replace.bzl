
def _replace_impl(ctx):
    out = ctx.actions.declare_file(ctx.attr.output_file)
    ctx.actions.expand_template(
        template = ctx.file.template,
        output = out,
        substitutions = ctx.attr.substitutions,
    )
    return [DefaultInfo(files = depset([out]))]

replace = rule(
    implementation = _replace_impl,
    attrs = {
        "template": attr.label(
            mandatory = True,
            allow_single_file = True,
        ),
        "output_file": attr.string(mandatory = True),
        "substitutions": attr.string_dict(
            mandatory = True,
            allow_empty = True,

        ),
    },
)